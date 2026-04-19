#include "Analysis/NounwindAnalysis.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"

#include <optional>

using namespace llvm;

namespace nounwind_lto {

static UnwindLattice join(UnwindLattice A, UnwindLattice B) {
  if (A == UnwindLattice::Throwing || B == UnwindLattice::Throwing)
    return UnwindLattice::Throwing;
  if (A == UnwindLattice::Unknown || B == UnwindLattice::Unknown)
    return UnwindLattice::Unknown;
  return UnwindLattice::Safe;
}

/// Embedded policy for libc++/Itanium-style runtime symbols that lack `nounwind`
/// in bitcode but have a fixed contract for this pipeline.
static std::optional<UnwindLattice> embeddedPolicyForName(StringRef Name) {
  if (Name.contains("__cxa_throw") ||
      Name.contains("__cxa_allocate_exception"))
    return UnwindLattice::Throwing;

  // Itanium `operator new` / `new[]` (including sized / aligned manglings).
  if (Name.starts_with("_Znwm") || Name.starts_with("_Znam"))
    return UnwindLattice::Safe;

  return std::nullopt;
}

static bool functionHasAnyInvoke(const Function &F) {
  for (const BasicBlock &BB : F)
    for (const Instruction &I : BB)
      if (isa<InvokeInst>(&I))
        return true;
  return false;
}

static bool functionHasNonNounwindInvoke(const Function &F) {
  for (const BasicBlock &BB : F)
    for (const Instruction &I : BB)
      if (const auto *Inv = dyn_cast<InvokeInst>(&I))
        if (!Inv->hasFnAttr(Attribute::NoUnwind))
          return true;
  return false;
}

/// When every `invoke` in F is `nounwind`, no unwind edge can fire into a
/// landing pad: the try region cannot throw, so `LandingPadInst`, `resume`, and
/// `llvm.eh.typeid.for` inside EH cleanup are dead for "does this function
/// unwind?" and must not force Throwing.
static bool tryRegionMayThrowViaInvoke(const Function &F) {
  if (!functionHasAnyInvoke(F))
    return true; // odd IR (e.g. resume with no invoke): stay conservative
  return functionHasNonNounwindInvoke(F);
}

static bool isEHThrowLike(const Instruction &I, const Function &F) {
  // Catch entry — not a throw; if all invokes are nounwind, this block is
  // unreachable from any unwind anyway.
  if (isa<LandingPadInst>(I))
    return false;

  if (const auto *Inv = dyn_cast<InvokeInst>(&I)) {
    if (Inv->hasFnAttr(Attribute::NoUnwind))
      return false;
    return true;
  }
  // Resume / typeid in dead catch cleanup: same predicate as each other.
  if (isa<ResumeInst>(I)) {
    return tryRegionMayThrowViaInvoke(F);
  }
  if (const auto *II = dyn_cast<IntrinsicInst>(&I)) {
    switch (II->getIntrinsicID()) {
    case Intrinsic::eh_typeid_for:
      return tryRegionMayThrowViaInvoke(F);
    default:
      break;
    }
  }
  return false;
}

static UnwindLattice latticeForDeclaration(const Function *F) {
  if (!F)
    return UnwindLattice::Unknown;
  if (std::optional<UnwindLattice> Pol = embeddedPolicyForName(F->getName()))
    return *Pol;
  if (F->hasFnAttribute(Attribute::NoUnwind))
    return UnwindLattice::Safe;
  return UnwindLattice::Unknown;
}

/// Callee lattice: SCC-local map (current iter) vs finished `Out`, vs decls.
static UnwindLattice lookupCallee(
    const Function *Callee,
    const DenseSet<const Function *> &SCCFuncs,
    const DenseMap<const Function *, UnwindLattice> &SCCState,
    const DenseMap<const Function *, UnwindLattice> &Out) {
  if (!Callee)
    return UnwindLattice::Unknown;

  if (Callee->hasFnAttribute(Attribute::NoUnwind))
    return UnwindLattice::Safe;

  if (std::optional<UnwindLattice> Pol = embeddedPolicyForName(Callee->getName()))
    return *Pol;

  if (SCCFuncs.count(Callee)) {
    auto It = SCCState.find(Callee);
    if (It != SCCState.end())
      return It->second;
    return UnwindLattice::Safe;
  }

  if (Callee->isDeclaration())
    return latticeForDeclaration(Callee);

  auto It = Out.find(Callee);
  if (It != Out.end())
    return It->second;

  // Defined in module but not in Out yet should not happen in bottom-up order.
  return UnwindLattice::Unknown;
}

static UnwindLattice transferFunction(
    const Function &F,
    const DenseSet<const Function *> &SCCFuncs,
    const DenseMap<const Function *, UnwindLattice> &SCCState,
    const DenseMap<const Function *, UnwindLattice> &Out) {
  UnwindLattice L = UnwindLattice::Safe;

  for (const BasicBlock &BB : F) {
    for (const Instruction &I : BB) {
      if (isEHThrowLike(I, F)) {
        L = join(L, UnwindLattice::Throwing);
        continue;
      }

      const auto *CB = dyn_cast<CallBase>(&I);
      if (!CB)
        continue;

      // Call site explicitly marked nounwind: cannot unwind along this edge.
      if (CB->hasFnAttr(Attribute::NoUnwind))
        continue;

      const Function *Callee = CB->getCalledFunction();
      if (!Callee) {
        L = join(L, UnwindLattice::Unknown);
        continue;
      }

      L = join(L, lookupCallee(Callee, SCCFuncs, SCCState, Out));
    }
  }

  return L;
}

static void solveSCC(ArrayRef<CallGraphNode *> Nodes,
                       DenseMap<const Function *, UnwindLattice> &Out) {
  DenseMap<const Function *, UnwindLattice> SCCState;
  DenseSet<const Function *> SCCFuncs;

  SmallVector<Function *, 8> Funcs;
  for (CallGraphNode *N : Nodes) {
    Function *F = N->getFunction();
    if (!F || F->isDeclaration())
      continue;
    Funcs.push_back(F);
    SCCFuncs.insert(F);
    SCCState[F] = UnwindLattice::Safe;
  }

  if (Funcs.empty())
    return;

  // Fixed-point for mutual recursion inside the SCC.
  bool Changed = true;
  while (Changed) {
    Changed = false;
    for (Function *F : Funcs) {
      UnwindLattice NewL = transferFunction(*F, SCCFuncs, SCCState, Out);
      UnwindLattice OldL = SCCState.lookup(F);
      if (NewL != OldL) {
        SCCState[F] = NewL;
        Changed = true;
      }
    }
  }

  for (Function *F : Funcs)
    Out[F] = SCCState[F];
}

void computeNounwindLattice(Module &M,
                            DenseMap<const Function *, UnwindLattice> &Out) {
  CallGraph CG(M);

  for (scc_iterator<CallGraph *> I = scc_begin(&CG), E = scc_end(&CG); I != E;
       ++I) {
    solveSCC(llvm::ArrayRef<llvm::CallGraphNode *>(*I), Out);
  }
}

} // namespace nounwind_lto
