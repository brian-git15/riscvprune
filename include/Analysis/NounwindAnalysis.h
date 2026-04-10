#pragma once

#include "llvm/IR/Function.h"

namespace nounwind_lto {

// Returns true if this function is considered safe to mark nounwind.
bool isFunctionNounwindSafe(const llvm::Function &F);

} // namespace nounwind_lto
