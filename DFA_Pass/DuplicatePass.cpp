#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"

using namespance llvm;
std::string readAnnotate(llvm::Function *f) {
  std::string annotation = "";

  // Get annotation variable
  GlobalVariable *glob =
      f->getParent()->getGlobalVariable("llvm.global.annotations");

  if (glob != NULL) {
    // Get the array
    if (ConstantArray *ca = dyn_cast<ConstantArray>(glob->getInitializer())) {
      for (unsigned i = 0; i < ca->getNumOperands(); ++i) {
        // Get the struct
        if (ConstantStruct *structAn =
                dyn_cast<ConstantStruct>(ca->getOperand(i))) {
          if (ConstantExpr *expr =
                  dyn_cast<ConstantExpr>(structAn->getOperand(0))) {
            // If it's a bitcast we can check if the annotation is concerning
            // the current function
            if (expr->getOpcode() == Instruction::BitCast &&
                expr->getOperand(0) == f) {
              ConstantExpr *note = cast<ConstantExpr>(structAn->getOperand(1));
              // If it's a GetElementPtr, that means we found the variable
              // containing the annotations
              if (note->getOpcode() == Instruction::GetElementPtr) {
                if (GlobalVariable *annoteStr =
                        dyn_cast<GlobalVariable>(note->getOperand(0))) {
                  if (ConstantDataSequential *data =
                          dyn_cast<ConstantDataSequential>(
                              annoteStr->getInitializer())) {
                    if (data->isString()) {
                      annotation += data->getAsString().lower() + " ";
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return annotation;
}


struct ModifyFunctionCallPass : public llvm::PassInfoMixin<ModifyFunctionCallPass> {
    llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
        for (auto &F : M){
            readAnnotate(&F);
            for (auto &BB : F) {
                for (auto &I : BB) {
                    if (auto *Call = llvm::dyn_cast<llvm::CallBase>(&I)) {
                        if (llvm::MDNode *Node = Call->getMetadata("custom_annotation")) {
                            // 获取元数据内容
                            llvm::StringRef Annotation = llvm::cast<llvm::MDString>(
                                Node->getOperand(0))->getString();
                            llvm::outs() << "Found metadata: " << Annotation << "\n";

                            // 修改函数调用名称
                            if (Call->getCalledFunction() &&
                                Call->getCalledFunction()->getName() == "strcpy") {
                                Call->getCalledFunction()->setName("strcpy_1");
                                llvm::outs() << "Renamed function call to strcpy_1\n";
                            }
                        }
                    }
                }
            }
        }
        return llvm::PreservedAnalyses::all();
    }
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "SVFAnalysisPass", LLVM_VERSION_STRING,
        [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name, llvm::ModulePassManager &MPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                    if (Name == "modify-function-call") {
                        MPM.addPass(ModifyFunctionCallPass());
                        return true;
                    }
                    return false;
                });
        }};
}