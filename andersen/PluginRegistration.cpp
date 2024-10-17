#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/PassManager.h"
#include "AndersonPointsToAnalysis.h" // Include your pass header

using namespace llvm::anderson;

llvm::PassPluginLibraryInfo getAndersonPointsToAnalysisPluginInfo() {
  llvm::PassPluginLibraryInfo PluginInfo;
  PluginInfo.APIVersion = LLVM_PLUGIN_API_VERSION;
  PluginInfo.PluginName = "AndersonPointsToAnalysis";
  PluginInfo.PluginVersion = LLVM_VERSION_STRING;

  PluginInfo.RegisterPassBuilderCallbacks = [](llvm::PassBuilder &PB) {
    PB.registerPipelineParsingCallback(
        [](llvm::StringRef Name, llvm::ModulePassManager &MPM,
           llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
          if (Name == "anderson") {
            MPM.addPass(llvm::anderson::AndersonPointsToAnalysis());
            return true;
          }
          return false;
        });
  };
  return PluginInfo;
}



extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getAndersonPointsToAnalysisPluginInfo();
}
