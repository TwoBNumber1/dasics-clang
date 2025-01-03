#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Pragma.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
using namespace clang;

// 1. RecursiveASTVisitor 用于遍历 AST 并找到目标函数
class FunctionMarkerVisitor : public RecursiveASTVisitor<FunctionMarkerVisitor> {
    public:
        explicit FunctionMarkerVisitor(ASTContext *Context) : Context(Context) {
            //llvm::outs() << "--- MarkFunctions Plugin:11111.\n";

        }

          // 重载 VisitCallExpr 函数
        bool VisitCallExpr(CallExpr *Call) {
            //direct call
            if (FunctionDecl *FD = Call->getDirectCallee()) {
                llvm::outs() << "Function call: " << FD->getNameInfo().getName().getAsString() << "\n";
                SourceLocation Loc = FD->getLocation();
                const SourceManager &SM = Context->getSourceManager();
            
                // if (FD->isDefined()) { 有没有函数体
                StringRef FileName = SM.getFilename(Loc);
                if (FileName.endswith("string.h")) {
                    // 构建 AttributeCommonInfo
                    AttributeCommonInfo Info(
                        SourceRange(Loc, Loc),                // 源范围
                        AttributeCommonInfo::Kind::AT_Annotate, // 属性种类 (AT_Annotate 表示注解属性)
                        AttributeCommonInfo::AS_GNU            // 属性的形式 (GNU 风格语法)
                        );
                    FD->addAttr(AnnotateAttr::Create(
                        (*Context), "duplicate", nullptr, 0,
                        Info
                    ));
                    llvm::outs() << "Marked function: " << FD->getName() << "\n" << FileName << "\n";
                    // 检查注解是否已附加
                    if (FD->hasAttr<AnnotateAttr>()) {
                        AnnotateAttr *Attr = FD->getAttr<AnnotateAttr>();
                        llvm::outs() << "Annotation found: " << Attr->getAnnotation() << "\n";
                    } else {
                        llvm::outs() << "No annotation found on " << FD->getNameAsString() << "\n";
                    }
                }
            }else{// indirect call 包括函数指针和虚函数（c++
                llvm::outs() << "Indirected call. \n";
            }
            return true;
        }

    private:
        ASTContext *Context;
    };

// 2. PragmaHandler 用于处理 `#pragma mark_functions`
class MarkFunctionsPragmaHandler : public PragmaHandler {
public:
    MarkFunctionsPragmaHandler() : PragmaHandler("mark_functions") {}

    void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer, Token &FirstToken) override {
        // 标记函数的逻辑在 AST 阶段完成
        llvm::outs() << "--- MarkFunctions Plugin : Handling #pragma mark_functions\n";
    }
};

// 3. ASTConsumer 集成 PragmaHandler 和 AST 访问逻辑
class FunctionMarkerASTConsumer : public ASTConsumer {
    public:
        explicit FunctionMarkerASTConsumer(ASTContext *Context) : Visitor(Context) {}

        void HandleTranslationUnit(ASTContext &Context) override {
            llvm::outs() << "--- MarkFunctions Plugin:Handling #pragma mark_functions HandleTranslationUnit...\n";
            Visitor.TraverseDecl(Context.getTranslationUnitDecl());
            llvm::outs() << "--- MarkFunctions Plugin:Finished Traversing AST.\n";
            //Context.getTranslationUnitDecl()->dump();
        }


        

    private:
        FunctionMarkerVisitor Visitor;
};


// 4. FrontendAction 注册 PragmaHandler 和 ASTConsumer
class FunctionMarkerFrontendAction : public PluginASTAction {
    protected:
        std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef) override {
            CI_ = &CI;
            // 添加 PragmaHandler
            CI.getPreprocessor().AddPragmaHandler(new MarkFunctionsPragmaHandler());
            llvm::outs() << "register FunctionMarkerConsumer \n" ;
            return std::make_unique<FunctionMarkerASTConsumer>(&(CI.getASTContext()));
        }

        // Automatically run the plugin after the main AST action
        // PluginASTAction::ActionType getActionType() override {
        //     llvm::outs() << "ActionType ... \n";
        //     return AddAfterMainAction;
        // }

        bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string> &args) override {
            return true; // 解析插件参数（无参数需求）
        }

        void PrintHelp(llvm::raw_ostream &ros) {
            ros << "Marks functions from <string.h> with 'duplicate' annotation using #pragma mark_functions\n";
        }

    private:
        clang::CompilerInstance *CI_;
};



// 5. 注册插件
static FrontendPluginRegistry::Add<FunctionMarkerFrontendAction> X1("mark-functions", "Mark <string.h> functions with metadata");
static PragmaHandlerRegistry::Add<MarkFunctionsPragmaHandler> P3("mark_functions", "Mark library header file functions with metadata");