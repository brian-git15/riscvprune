#include "Transform/NounwindPass.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "nounwind-lto", "0.1",
          [](PassBuilder &PB) {
            auto addNounwindPass = [](ModulePassManager &MPM) {
              MPM.addPass(nounwind_lto::NounwindPass());
            };

            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "nounwind-lto") {
                    MPM.addPass(nounwind_lto::NounwindPass());
                    return true;
                  }
                  return false;
                });

            // FullLTO only for Phase 1 WPV. ThinLTO remains intentionally out of
            // scope until we add summary-aware analysis behavior.
            PB.registerFullLinkTimeOptimizationLastEPCallback(
                [addNounwindPass](ModulePassManager &MPM,
                                  OptimizationLevel) { addNounwindPass(MPM); });
            PB.registerOptimizerLastEPCallback(
                [addNounwindPass](ModulePassManager &MPM,
                                  OptimizationLevel) { addNounwindPass(MPM); });
          }};
}
