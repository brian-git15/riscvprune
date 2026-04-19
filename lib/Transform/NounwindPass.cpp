#include "Transform/NounwindPass.h"

#include "Analysis/NounwindAnalysis.h"

#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include "llvm/ADT/SmallVector.h"

namespace nounwind_lto {

static bool invokeCalleeIsNounwind(const llvm::InvokeInst &Inv) {
  if (llvm::Function *Callee = Inv.getCalledFunction())
    return Callee->hasFnAttribute(llvm::Attribute::NoUnwind);
  return Inv.hasFnAttr(llvm::Attribute::NoUnwind);
}

/// Replace invokes whose callee is nounwind with call + branch to the normal
/// successor (drops the unwind edge so EH blocks can go dead).
static bool lowerNounwindInvokes(llvm::Module &M) {
  llvm::SmallVector<llvm::InvokeInst *, 16> Invokes;
  for (llvm::Function &F : M)
    for (llvm::BasicBlock &BB : F)
      for (llvm::Instruction &I : BB)
        if (auto *Inv = llvm::dyn_cast<llvm::InvokeInst>(&I))
          Invokes.push_back(Inv);

  bool Changed = false;
  for (llvm::InvokeInst *Inv : Invokes) {
    if (!Inv->getParent())
      continue;
    if (!invokeCalleeIsNounwind(*Inv))
      continue;

    llvm::IRBuilder<> B(Inv);
    llvm::SmallVector<llvm::Value *, 8> Args;
    Args.append(Inv->arg_begin(), Inv->arg_end());

    llvm::CallInst *Call = B.CreateCall(Inv->getFunctionType(),
                                        Inv->getCalledOperand(), Args);
    // LLVM 18: getTailCallKind() exists on CallInst only, not InvokeInst. This
    // lowering replaces invoke with call+br; use no tail marker (valid for IR).
    Call->setTailCallKind(llvm::CallInst::TailCallKind::TCK_None);
    Call->setCallingConv(Inv->getCallingConv());
    Call->setAttributes(Inv->getAttributes());
    Call->copyMetadata(*Inv);
    Call->setDebugLoc(Inv->getDebugLoc());
    if (auto FMF = Inv->getFastMathFlags(); FMF.any())
      Call->setFastMathFlags(FMF);

    Inv->replaceAllUsesWith(Call);
    llvm::BranchInst::Create(Inv->getNormalDest(), Inv);
    Inv->eraseFromParent();
    Changed = true;
  }

  return Changed;
}

llvm::PreservedAnalyses
NounwindPass::run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
  llvm::DenseMap<const llvm::Function *, UnwindLattice> Lat;
  computeNounwindLattice(M, Lat);

  bool Changed = false;
  for (llvm::Function &F : M) {
    if (F.isDeclaration() || F.hasFnAttribute(llvm::Attribute::NoUnwind))
      continue;

    auto It = Lat.find(&F);
    if (It != Lat.end() && It->second == UnwindLattice::Safe) {
      F.addFnAttr(llvm::Attribute::NoUnwind);
      Changed = true;
    }
  }

  if (lowerNounwindInvokes(M))
    Changed = true;

  return Changed ? llvm::PreservedAnalyses::none()
                 : llvm::PreservedAnalyses::all();
}

} // namespace nounwind_lto
