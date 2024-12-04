#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IntrinsicInst.h" // 用于检查内建函数

#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "WPA/Andersen.h"
#include "Graphs/SVFG.h"
#include "Util/Options.h"
#include "MSSA/MemRegion.h"
#include "MemoryModel/PointerAnalysisImpl.h"
using namespace llvm;
using namespace SVF;

namespace {

struct SVFAnalysisPass : public PassInfoMixin<SVFAnalysisPass> {
    SVFAnalysisPass() = default;

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
        std::vector<std::string> moduleNameVec;
        auto fileName = M.getSourceFileName();
        auto bitcodeName = M.getModuleIdentifier();
        moduleNameVec.push_back(bitcodeName);

        errs() << "File name: " << fileName << "\nBitcode name: " << bitcodeName << "\n";

        //构建PAG (SVFIR)
        SVFModule* svfModule = LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);
        SVFIRBuilder builder(svfModule);
        SVFIR* pag = builder.build();
        Andersen* ander = AndersenWaveDiff::createAndersenWaveDiff(pag);

        // Sparse value-flow graph (SVFG) 这里数据流敏感了
        SVFGBuilder svfBuilder;
        SVFG* svfg = svfBuilder.buildFullSVFG(ander);
        
        // MemSSA* mssa = svfg->getMSSA();
        // BVDataPTAImpl* pta = mssa->getPTA();

        //遍历所有函数调用点的参数列表
        for(SVFIR::CSToArgsListMap::iterator it = pag->getCallSiteArgsMap().begin(),
            eit = pag->getCallSiteArgsMap().end(); it!=eit; ++it){
            SVF::CallGraph::FunctionSet callees;
            ander->getCallGraph()->getCallees(it->first,callees);
             
            for(SVF::CallGraph::FunctionSet::const_iterator cit = callees.begin(), ecit = callees.end(); cit!=ecit; cit++){
                const SVFFunction* fun = *cit;
                outs() << "Callee Name: " << "memcpy?"<< "\n";
                SVFIR::SVFVarList &arglist = it->second;
                assert(!arglist.empty()	&& "no actual parameter at deallocation site?");
                //只查看指针参数 形参不看
                for (SVFIR::SVFVarList::const_iterator ait = arglist.begin(), aeit = arglist.end(); ait != aeit; ++ait){
                    const PAGNode *pagNode = *ait;
                    if (pagNode->isPointer()){
                        const SVFGNode *snk = svfg->getActualParmVFGNode(pagNode, it->first);
                        // 打印实际参数节点的ID
                        outs() << "SVFG Node ID: " << snk->getId() <<" Name:" << snk->getFun()->getName() << "\n";
                        outs() << "Original parameter name: " << pagNode->getValue()->getName() << "\n";
                           // 调用 CollectPtsChain 方法，获取 Points-To 链

  
                        //多级函数指针情况处理
                        /*
                            SVFStmt::SVFStmtSetTy& loads = const_cast<PAGNode*>(pagNode)->getOutgoingEdges(SVFStmt::Load);
                            for(const SVFStmt* ld : loads)
                            {
                                if(SVFUtil::isa<DummyValVar>(ld->getDstNode()))
                                    addToSinks(getSVFG()->getStmtVFGNode(ld));
                        }*/
                    }
                }
                
            }
        }

        /*
        for (Function &F : M) {
            if (!F.isDeclaration()) {
                errs() << "Function: " << F.getName() << "\n";
            }
             // 遍历Module中的每条指令
            for (Instruction &I : instructions(F)) {
                // 如果是函数调用指令
                if (CallInst* callInst = dyn_cast<CallInst>(&I)) {
                    // 跳过内建函数 内建函数要分析吗？不用吧
                    if (callInst->getIntrinsicID() != llvm::Intrinsic::not_intrinsic) {
                        errs() << "Skipping intrinsic function: " << *callInst << "\n";
                        continue;
                    }
                    errs() << "Analyzing call instruction: " << *callInst << "\n";
                     // 获取调用点的所有参数
                    for (unsigned i = 0; i < callInst->getNumOperands(); ++i) {
                            Value* arg = callInst->getArgOperand(i);
                            errs() << "Analyzing argument: " << *arg << "\n";  
                    }
                }
            }
            
        }*///这里是利用LLVM的方法
        //delete pag; 会dump 暂时comment掉
        return PreservedAnalyses::all();
    }
};

} // end of anonymous namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "SVFAnalysisPass", LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "svf-analysis-pass") {
                        MPM.addPass(SVFAnalysisPass());
                        return true;
                    }
                    return false;
                });
        }};
}