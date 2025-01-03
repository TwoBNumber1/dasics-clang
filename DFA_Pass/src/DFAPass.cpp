#include <string>
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IntrinsicInst.h" // 用于检查内建函数

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/Support/raw_ostream.h"

#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "WPA/Andersen.h"
#include "Graphs/SVFG.h"
#include "Util/Options.h"
#include "MSSA/MemRegion.h"
#include "MemoryModel/PointerAnalysisImpl.h"
using namespace llvm;

/// Returns the string representation of a llvm::Value* or llvm::Type*  
template <typename T> static std::string Print(T* value_or_type) {
    std::string str;
    llvm::raw_string_ostream stream(str);
    value_or_type->print(stream);
    return str;
}

// 根据函数名称Name在当前Module中查找相关的函数声明
llvm::Function *getFunction(Module &M, const std::string &Name, llvm::Type *RetType, llvm::ArrayRef<llvm::Type *> ParamTypes) {
    // 查找是否已有声明
    if (auto *F = M.getFunction(Name)) {
        return F; // 如果已存在，直接返回
    }

    // 如果不存在，创建新的声明
    llvm::FunctionType *FuncType = llvm::FunctionType::get(RetType, ParamTypes, false);
    llvm::Function *NewFunc = llvm::Function::Create(FuncType, llvm::Function::ExternalLinkage, Name, &M);
    // 添加修饰符
    for (auto &Arg : NewFunc->args()) {
        Arg.addAttr(llvm::Attribute::AttrKind::NoUndef); // 添加 noundef
    }
    // 设置返回值修饰符（如果需要）
    if (NewFunc->getReturnType()->isIntegerTy(32)) { // 对返回值为 i32 的函数设置 signext
        NewFunc->addRetAttr(llvm::Attribute::AttrKind::SExt);
    }
    return NewFunc;
}


bool is_declared = false;
using namespace SVF;


int Counter = 0; // 用于生成唯一变量名的计数器
typedef Map<NodeID, PointsTo> NodeToPTSSMap;
typedef FIFOWorkList<NodeID> WorkList;
namespace {
    /**
     * @author Yue.C
     * 根据指定的节点列出Point-to chain
     */
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
            //TODO: 应该通过某种方式只check specified的库函数调用点
            int count = 0;
            for(SVFIR::CSToArgsListMap::iterator it = pag->getCallSiteArgsMap().begin(),
                eit = pag->getCallSiteArgsMap().end(); it!=eit; ++it){
                const Value *callsiteV = LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(it->first->getCallSite());
                //if (!llvmValue) continue;
                outs() <<" ------ Current Callsite Value: "<< Print(callsiteV) <<" ------ \n";
                SVF::CallGraph::FunctionSet callees; 
                ander->getCallGraph()->getCallees(it->first,callees);

                for(SVF::CallGraph::FunctionSet::const_iterator cit = callees.begin(), ecit = callees.end(); cit!=ecit; cit++){
                    const SVFFunction* fun = *cit;
                    //skip llvm intrinsic function eg.. llvm.memcpy...
                    if(fun->isIntrinsic() ||  (fun->toString().find("llvm") != std::string::npos)) {
                        continue;
                    }
                    outs() << "Callee Name: " << fun->toString() << "\n";
                    SVFIR::SVFVarList &arglist = it->second;
                    // 判断当前函数调用是否传递实参或无参数，否则说明只需要做栈保护啊
                    assert(!arglist.empty()	&& "no actual parameter at deallocation site?");
                    // 遍历调用点的参数列表
                    outs() << " Callee FunctionArgs iterator   -------: \n";
                    for (SVFIR::SVFVarList::const_iterator ait = arglist.begin(), aeit = arglist.end(); ait != aeit; ++ait){
                        const PAGNode *pagNode = *ait;
                        //只查看指针参数 形参不看
                        if (pagNode->isPointer()){
                            const SVFGNode *snk = svfg->getActualParmVFGNode(pagNode, it->first);
                            outs() << "pagNode:" << pagNode->toString() << "\n";

                            // 打印实际参数节点的ID
                            outs() << "1. 当前实参的SVFG Node ID: " << snk->getId() << " Name:" << snk->getFun()->getName() << "\n";
                            
                            const Value *V = LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(pagNode->getValue());
                            outs() << "2. Original parameter name: " << V << "\n";
                            outs() << "3. Pag->toString():" << pagNode->toString() << "\n";

                            if (!V) {
                                errs() << "No LLVM Value associated with NodeID " << pagNode->toString() << "\n";
                                return PreservedAnalyses::all();
                            }

                            // 获取 Value 的类型
                            Type *Ty = V->getType();
                            errs() << "Type of Value: ";
                            Ty->print(errs());
                            errs() << "\n";
                            const DataLayout &DL = M.getDataLayout();
                            //获取指向数据对象长度
                            uint64_t TypeSize;
                            if (const auto *PtrTy = llvm::dyn_cast<llvm::PointerType>(Ty)) {//指针类型需要获取指向什么样的结构体对象
                                // 使用 GEP 或上下文获取元素类型
                                if (const llvm::GetElementPtrInst *GEP = llvm::dyn_cast<llvm::GetElementPtrInst>(V)) {
                                    llvm::Type *ElementType = GEP->getSourceElementType();
                                    errs() << "Element Type of Pointer (from GEP): ";
                                    ElementType->print(errs());
                                    errs() << "\n";

                                    TypeSize = DL.getTypeSizeInBits(ElementType) / 8;
                                    errs() << "Element Type Size (bytes): " << TypeSize << "\n";
                                } else {
                                    errs() << "Cannot determine element type from opaque pointer.\n";
                                }
                            } else {
                                // 获取元素类型的大小（以字节为单位）
                                TypeSize = DL.getTypeSizeInBits(Ty) / 8;
                                errs() << "Type Size (bytes): " << TypeSize << "\n";
                            }
                            llvm::LLVMContext &Context = M.getContext();
                            PointerType *PrintfArgTy = PointerType::getUnqual(Type::getInt8Ty(Context));
                            FunctionType *PrintfTy = FunctionType::get(
                                IntegerType::getInt32Ty(Context),
                                PrintfArgTy,
                                /*IsVarArgs=*/true);

                            FunctionCallee Printf = M.getOrInsertFunction("printf", PrintfTy);

                            // Set attributes as per inferLibFuncAttributes in BuildLibCalls.cpp
                            Function *PrintfF = dyn_cast<Function>(Printf.getCallee());
                            PrintfF->setDoesNotThrow();
                            PrintfF->addParamAttr(0, Attribute::NoCapture);
                            PrintfF->addParamAttr(0, Attribute::ReadOnly);

                             // ------------------------------------------------------------------------
                                llvm::Constant *PrintfFormatStr = llvm::ConstantDataArray::getString(
                                    Context, "(llvm-tutor) Hello from: %s\n(llvm-tutor)   number of arguments: %d\n");

                                Constant *PrintfFormatStrVar =
                                    M.getOrInsertGlobal("PrintfFormatStr", PrintfFormatStr->getType());
                                dyn_cast<GlobalVariable>(PrintfFormatStrVar)->setInitializer(PrintfFormatStr);

                            
                            
                            if (auto *Call1 = llvm::dyn_cast<CallInst>(callsiteV) ) {
                                
                                //去除const标志
                                
                                auto *Call = const_cast<llvm::CallInst*>(Call1);
                                llvm::IRBuilder<> Builder(Call);
                                auto FuncName = Builder.CreateGlobalStringPtr("Died.");
                                llvm::Value *FormatStrPtr = Builder.CreatePointerCast(PrintfFormatStrVar, PrintfArgTy, "formatStr");
                                Builder.CreateCall(
                                Printf, {FormatStrPtr, FuncName, Builder.getInt32(5)});
                                count++;
                                if(count > 0) break;
                                outs() << "generate alloc and free function pair....\n";
                                

                                // 获取或创建 dasics_libcfg_alloc 函数
                                llvm::Type *RetType = llvm::Type::getInt32Ty(Context); // 返回类型
                                SmallVector<llvm::Type *> ParamTypes{
                                    llvm::Type::getInt64Ty(Context), // 权限类型 (uint64_t)
                                    
                                    llvm::Type::getInt64Ty(Context), // 起始地址 (uint64_t)
                                    llvm::Type::getInt64Ty(Context)  // 结束地址 (uint64_t)
                                };
                                FunctionType *AllocFuncType = FunctionType::get(llvm::Type::getInt32Ty(Context),
                                             ParamTypes, false);
                                // 1. 创建函数属性集合 (为空)
                                AttributeSet AllocFnAttrs; // 默认构造函数创建一个空的 AttributeSet

                                // 2. 创建返回值属性集合
                                llvm::AttrBuilder AllocB1(Context);
                                AllocB1.addAttribute(llvm::Attribute::SExt);
                                AttributeSet AllocRetAttrs = AttributeSet::get(Context, AllocB1);

                                // 3. 创建参数属性集合
                                llvm::AttrBuilder AllocB2(Context);
                                AllocB2.addAttribute(llvm::Attribute::NoUndef);
                                AttributeSet AllocArg1Attrs = AttributeSet::get(Context,  AllocB2);
                                SmallVector<AttributeSet, 3> AllocArgAttrs;
                                //AttributeSet AllocArg1Attr = AttributeSet::get(Context,  FreeB2);
                                AllocArgAttrs.push_back(AllocArg1Attrs);
                                AllocArgAttrs.push_back(AllocArg1Attrs);
                                AllocArgAttrs.push_back(AllocArg1Attrs);
                                ArrayRef<AttributeSet> AllocArgAttrsRef = AllocArgAttrs;
                                llvm::AttributeList AllocAttrs = llvm::AttributeList::get(
                                        Context, AllocFnAttrs, AllocRetAttrs, AllocArgAttrs);
                                //Function AllocFunc = Function::Create("dasics_libcfg_alloc", AllocFuncType, AllocAttrs);
                                FunctionCallee AllocFunc = M.getOrInsertFunction("dasics_libcfg_alloc", AllocFuncType, AllocAttrs);
                                //(M, "dasics_libcfg_alloc", RetType, ParamTypes);
                                // 获取或创建 dasics_libcfg_free 函数
                                //llvm::Type *RetTypeFree = ; // 返回类型 i32
                                SmallVector<llvm::Type *> ParamTypesFree{
                                    llvm::Type::getInt32Ty(M.getContext()) // 参数类型 i32
                                };
                                FunctionType *FreeFuncType = FunctionType::get(llvm::Type::getInt32Ty(Context),
                                             ParamTypesFree, false);


                                // 1. 创建函数属性集合 (为空)
                                AttributeSet FreeFnAttrs; // 默认构造函数创建一个空的 AttributeSet

                                // 2. 创建返回值属性集合
                                llvm::AttrBuilder FreeB1(Context);
                                FreeB1.addAttribute(llvm::Attribute::SExt);
                                AttributeSet FreeRetAttrs = AttributeSet::get(Context, FreeB1);

                                // 3. 创建参数属性集合
                                llvm::AttrBuilder FreeB2(Context);
                                FreeB2.addAttribute(llvm::Attribute::SExt);
                                FreeB2.addAttribute(llvm::Attribute::NoUndef);
                                SmallVector<AttributeSet, 1> FreeArgAttrs;
                                AttributeSet FreeArgAttr = AttributeSet::get(Context,  FreeB2);
                                FreeArgAttrs.push_back(FreeArgAttr);
                                ArrayRef<AttributeSet> FreeArgAttrsRef = FreeArgAttrs;
                                llvm::AttributeList FreeAttrs = llvm::AttributeList::get(
                                        Context, FreeFnAttrs, FreeRetAttrs, FreeArgAttrsRef);
                                
                                FunctionCallee FreeFunc = M.getOrInsertFunction("dasics_libcfg_free", FreeFuncType, FreeAttrs);
                                
                                outs() << "Declare over ..." <<"\n";
                                

                                // 创建alloc调用所需要的参数
                                llvm::Value *Permissions = llvm::ConstantInt::get(llvm::Type::getInt64Ty(Context), 7); // 示例权限值
                                llvm::Value *StartAddr = llvm::ConstantInt::get(llvm::Type::getInt64Ty(Context), 0x1000); // 示例起始地址
                                llvm::Value *EndAddr = llvm::ConstantInt::get(llvm::Type::getInt64Ty(Context), 0x2000);   // 示例结束地址

                                // 创建调用指令
                                Builder.SetInsertPoint(Call);
                                CallInst  *AllocCall = Builder.CreateCall(AllocFunc, {Permissions, StartAddr, EndAddr});
                                AllocCall->setAttributes(AllocAttrs);
                                // 在 TargetCall 后插入 free 调用
                                llvm::Instruction *NextInst = Call->getNextNode(); // 获取 TargetCall 后的指令
                                if (NextInst) {
                                    Builder.SetInsertPoint(NextInst); // 插入到下一条指令之前
                                } else {
                                    llvm::BasicBlock *ParentBB = Call->getParent();
                                    Builder.SetInsertPoint(ParentBB); // 如果没有后续指令，插入到基本块的末尾
                                }
                                Builder.CreateCall(FreeFunc, {AllocCall});

                            }
                            

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

                            //TODO: 多级指针情况处理
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
                outs() <<" ------ Current Callsite Value: "<< Print(callsiteV) <<" end ------ \n";
            }
            /**
             * TODO: 在函数调用点往上找安全调用的代码，做参数填充(decre) 在当前指令之前插入
             * 1. 获得标注的函数调用指令查看参数，仅对实参做记录
             * 2. 根据记录的实参在标注指令之前挨个插入dasics_libcfg_alloc()
             * 3. 替换lib_call指令
             * 4. 根据2步骤的情况在标注指令之后挨个插入dasics_libcfg_free()
             */
            
            for (Function &F : M) {
                for (BasicBlock &BB : F) {
                    for (Instruction &I : BB) {
                        // 判断是否是 CallInst
                        if (auto *Call = dyn_cast<CallInst>(&I)) {
                            // 判断调用的函数是否是 @dasics_libcfg_alloc
                            if (Function *Callee = Call->getCalledFunction()) {
                                if (Callee->getName() == "dasics_libcfg_alloc") {
                                    errs() << "Found call to dasics_libcfg_alloc:\n";
                                    Call->print(errs());
                                    errs() << "\n";
                                    // 获取原始的第 3 个参数
                                    Value *OldArg = Call->getArgOperand(2);
                                    // 创建新的值作为替换，例如将其替换为常量整数 42
                                    // 换之前分析出来的大小
                                    llvm::IRBuilder<> Builder(Call);
                                    Value *NewArg = Builder.getInt64(42);
                                    // 替换第 3 个参数
                                    Call->setArgOperand(2, NewArg);
                                    errs() << "Replaced third argument in call to 'dasics_libcfg_alloc'.\n";
                                    Call->print(errs());
                                }
                            }
                        }
                    }
                }
            }

            //这里是利用LLVM的方法
            //delete pag; 会dump 暂时comment掉
            /**
             * Analysis的返回值要注意
             * return PreservedAnalyses::all() 表示未修改IR 将会丢弃所有transform
             */
            
            return PreservedAnalyses::none();
        }
    };


    /**
     * @author: Yue.C
     * 栈保护需要栈深度数据，该Pass的作用将会在栈深度相关指令处把sp的数据load出来塞进栈保护调用中，
     * 参数将被
     */
    struct StackProtectPass : public MachineFunctionPass {
        static char ID;
        StackProtectPass() : MachineFunctionPass(ID) {
        
        }

        bool runOnMachineFunction(MachineFunction &MF) override {
            const TargetFrameLowering *TFL = MF.getSubtarget().getFrameLowering();
            if (!TFL) {
                errs() << "No frame lowering available.\n";
                return false;
            }

            // 计算栈深度
            int64_t StackSize = MF.getFrameInfo().getStackSize();
            errs() << "Function: " << MF.getName() << ", Stack Size: " << StackSize << "\n";

            // 遍历指令并修改安全保护调用
            for (auto &MBB : MF) {
                for (auto &MI : MBB) {
                    if (MI.isCall()) {
                        if (MI.getOperand(0).isGlobal() &&
                            MI.getOperand(0).getGlobal()->getName() == "protect_func") {
                            // 修改指令，回填栈深度
                            MachineInstrBuilder MIB = BuildMI(MBB, MI, MI.getDebugLoc(), MI.getDesc());
                            MIB.addImm(StackSize); // 添加栈深度作为参数
                            MI.eraseFromParent();  // 删除旧指令
                            errs() << "Updated call to protect_func with stack depth.\n";
                            break;
                        }
                    }
                }
            }
            return true;
        }
    };
} // end of anonymous namespace

/**
 * Pass注册
 * 
 */
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "SVFAnalysisPass", LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            //Register ModulePass
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    //这里的name都是调用的时候要填的参数名称
                    if (Name == "svf-analysis-pass") { 
                        MPM.addPass(SVFAnalysisPass());
                        return true;
                    }
                    return false;
                });
        }};
}