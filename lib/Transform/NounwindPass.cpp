#include "Transform/NounwindPass.h"

#include "Analysis/NounwindAnalysis.h"

#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

namespace nounwind_lto {

llvm::PreservedAnalyses
NounwindPass::run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
  bool Changed = false;

  for (llvm::Function &F : M) {
    if (F.isDeclaration() || F.hasFnAttribute(llvm::Attribute::NoUnwind)) {
      continue;
    }

    if (isFunctionNounwindSafe(F)) {
      F.addFnAttr(llvm::Attribute::NoUnwind);
      Changed = true;
    }
  }

  return Changed ? llvm::PreservedAnalyses::none()
                 : llvm::PreservedAnalyses::all();
}

} // namespace nounwind_lto
