#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Module.h"

namespace nounwind_lto {

/// Exception-effect lattice for nounwind inference.
/// - Safe:    no unwind/throw paths under this model.
/// - Unknown: may unwind (e.g. unknown callee, declaration without nounwind).
/// - Throwing: definitely unwinds (EH instructions or proven bad callees).
///
/// Library symbols without LLVM `nounwind` are refined by an embedded policy
/// (see NounwindAnalysis.cpp): known EH allocation/throw sinks vs. assumed-trap
/// allocation (e.g. Itanium `operator new`).
///
/// EH: when every `invoke` in a function is `nounwind`, landing pads / `resume` /
/// `llvm.eh.typeid.for` are treated as dead for the unwind lattice (catch-only).
enum class UnwindLattice { Safe, Unknown, Throwing };

/// Bottom-up CallGraph SCC analysis (same SCC walk as CallGraphSCCPass):
/// for each SCC in reverse topological order of the SCC DAG, solve a local
/// fixed-point for mutually recursive functions, then record results.
void computeNounwindLattice(llvm::Module &M,
                            llvm::DenseMap<const llvm::Function *, UnwindLattice>
                                &Out);

} // namespace nounwind_lto
