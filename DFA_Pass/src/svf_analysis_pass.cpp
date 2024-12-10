#include <string>
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
typedef Map<NodeID, PointsTo> NodeToPTSSMap;
typedef FIFOWorkList<NodeID> WorkList;
namespace {

PointsTo& CollectPtsChain(SVFG* svfg, BVDataPTAImpl* pta, NodeID id, NodeToPTSSMap& cachedPtsMap)
{
    SVFIR* pag = svfg->getPAG();

    NodeID baseId = pag->getBaseObjVar(id);
    NodeToPTSSMap::iterator it = cachedPtsMap.find(baseId);
    if(it!=cachedPtsMap.end())
    {
        return it->second;
    }
    else
    {
        PointsTo& pts = cachedPtsMap[baseId];
        // base object
        if (!Options::CollectExtRetGlobals())
        {
            if(pta->isFIObjNode(baseId) && pag->getGNode(baseId)->hasValue())
            {
                ValVar* valVar = SVFUtil::dyn_cast<ValVar>(pag->getGNode(baseId));
                if(valVar && valVar->getGNode() && SVFUtil::isExtCall(SVFUtil::cast<ICFGNode>(valVar->getGNode())))
                {
                    return pts;
                }
            }
        }

        pts |= pag->getFieldsAfterCollapse(baseId);

        WorkList worklist;
        for(PointsTo::iterator it = pts.begin(), eit = pts.end(); it!=eit; ++it)
            worklist.push(*it);

        while(!worklist.empty())
        {
            NodeID nodeId = worklist.pop();
            const PointsTo& tmp = pta->getPts(nodeId);
            for(PointsTo::iterator it = tmp.begin(), eit = tmp.end(); it!=eit; ++it)
            {
                pts |= CollectPtsChain(svfg, pta,*it,cachedPtsMap);
            }
        }
        return pts;
    }
}


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

        // Sparse value-flow graph (SVFG) 这里做数据流敏感了
        SVFGBuilder svfBuilder;
        SVFG* svfg = svfBuilder.buildFullSVFG(ander);
        MemSSA* mssa = svfg->getMSSA();
        BVDataPTAImpl* BVpta = mssa->getPTA();

        //遍历所有函数调用点
        for(SVFIR::CSToArgsListMap::iterator it = pag->getCallSiteArgsMap().begin(),
            eit = pag->getCallSiteArgsMap().end(); it!=eit; ++it){
            SVF::CallGraph::FunctionSet callees;
            ander->getCallGraph()->getCallees(it->first,callees);

            for(SVF::CallGraph::FunctionSet::const_iterator cit = callees.begin(), ecit = callees.end(); cit!=ecit; cit++){
                const SVFFunction* fun = *cit;
                //skip llvm intrinsic function
                if(fun->isIntrinsic() ||  (fun->toString().find("llvm") != std::string::npos)) {
                    continue;
                }
                outs() << "Callee Name: " << fun->toString() << "\n";
                SVFIR::SVFVarList &arglist = it->second;
                //判断当前函数调用是否传递实参或无参数
                assert(!arglist.empty()	&& "no actual parameter at deallocation site?");
                //遍历调用点的参数列表
                outs() << " Callee FunctionArgs iterator     -------: \n";
                for (SVFIR::SVFVarList::const_iterator ait = arglist.begin(), aeit = arglist.end(); ait != aeit; ++ait){
                    const PAGNode *pagNode = *ait;
                    //只查看指针参数 形参不看
                    if (pagNode->isPointer()){
                        const SVFGNode *snk = svfg->getActualParmVFGNode(pagNode, it->first);
                        outs() << "pagNode:" << pagNode->toString() << "\n";

                        // 打印实际参数节点的ID
                        outs() << "1. 当前实参的SVFG Node ID: " << snk->getId() << " Name:" << snk->getFun()->getName() << "\n";
                        outs() << "2. Original parameter name: " << pagNode->getValue() << "\n";
                        outs() << "3. Pag->toString():" << pagNode->toString() << "\n";
                        // 调用 CollectPtsChain 方法，获取 Points-To 链
                        const PointsTo& pts = ander->getPts(snk->getId());
                        outs() << "4. ---------  迭代当前实参的Point-to Set -----------\n";
                        //如果没有point-to 只看PAGNode本身的value就可以了（一个define statement
                        for (PointsTo::iterator ii = pts.begin(), ie = pts.end();ii != ie; ii++){   
                            outs() << "GNode: " << *ii << " \n";
                            PAGNode* targetObj = pag->getGNode(*ii);
                            NodeToPTSSMap cachedPtsMap;
                            PointsTo& ptsChain = CollectPtsChain(svfg, BVpta, targetObj->getId() , cachedPtsMap);
                            for (PointsTo::iterator ptc = ptsChain.begin(), ptce = ptsChain.end();ptc != ptce; ptc++){
                                outs() << "ptsChain:" << *ptc << " \n";
                                PAGNode* a =  pag->getGNode(*ptc);
                                if(a->hasValue()){
                                    outs() << "ptsChain -> :" << a->getValue()->toString() << ")\t" << a->toString()  << "\n";
                                }
                            }
                        }
                        outs() << "4. ---------  end -----------\n";



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