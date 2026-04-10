#pragma once

#include "llvm/IR/PassManager.h"

namespace nounwind_lto {

class NounwindPass : public llvm::PassInfoMixin<NounwindPass> {
public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM);
};

} // namespace nounwind_lto
