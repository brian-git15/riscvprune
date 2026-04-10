#include "Analysis/NounwindAnalysis.h"

#include "llvm/IR/Instructions.h"

namespace nounwind_lto {

bool isFunctionNounwindSafe(const llvm::Function &F) {
  if (F.isDeclaration()) {
    return false;
  }

  for (const llvm::BasicBlock &BB : F) {
    for (const llvm::Instruction &I : BB) {
      if (const auto *CB = llvm::dyn_cast<llvm::CallBase>(&I)) {
        if (const llvm::Function *Callee = CB->getCalledFunction()) {
          if (!Callee->hasFnAttribute(llvm::Attribute::NoUnwind)) {
            return false;
          }
        } else {
          // Conservative for indirect calls until policy is added.
          return false;
        }
      }
    }
  }

  return true;
}

} // namespace nounwind_lto
