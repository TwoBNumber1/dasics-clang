//==============================================================================
// FILE:
//    CodeRefactor.cpp
//
// DESCRIPTION: CodeRefactor will rename a specified member method in a class
// (or a struct) and in all classes derived from it. It will also update all
// call sites in which the method is used so that the code remains semantically
// correct. For example we can use CodeRefactor to rename Base::foo as
// Base::bar.
//
// USAGE:
//    1. As a loadable Clang plugin:
//      clang -cc1 -load <BUILD_DIR>/lib/libCodeRefactor.dylib  -plugin  '\'
//      CodeRefactor -plugin-arg-CodeRefactor -class-name '\'
//      -plugin-arg-CodeRefactor Base  -plugin-arg-CodeRefactor -old-name '\'
//      -plugin-arg-CodeRefactor run  -plugin-arg-CodeRefactor -new-name '\'
//      -plugin-arg-CodeRefactor walk test/CodeRefactor_Class.cpp
//    2. As a standalone tool:
//       <BUILD_DIR>/bin/ct-code-refactor --class-name=Base --new-name=walk '\'
//        --old-name=run test/CodeRefactor_Class.cpp
//
// License: The Unlicense
//==============================================================================
#include "CodeRefactor.h"

#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Tooling/Refactoring/Rename/RenamingAction.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

using namespace clang;
using namespace ast_matchers;

//-----------------------------------------------------------------------------
// CodeRefactorMatcher - implementation
//-----------------------------------------------------------------------------

class CodeRefactorASTVisitor : public RecursiveASTVisitor<CodeRefactorASTVisitor> {
public:
    explicit CodeRefactorASTVisitor(ASTContext &Context) : Context(Context) {}

    // bool VisitFunctionDecl(FunctionDecl *FuncDecl) {
    //     // 修改函数名为 "printf"
    //     if (FuncDecl->getNameInfo().getName().getAsString() == "A") {
    //         FuncDecl->setName(Context->Idents.get("printf"));
    //     }
    //     return true;
    // }

    bool VisitCallExpr(CallExpr *Call) {
        // 修改函数调用的函数名为 "printf"
        llvm::outs() << "Handing CallExpr: " << Call->getDirectCallee()->getNameInfo().getAsString() << "\n";
        if (FunctionDecl *FD = Call->getDirectCallee()) {
            if(FD->getNameInfo().getAsString() == "A") {
                llvm::outs() << "Before Modifty: Function Name: " << FD->getNameAsString() << "\n";
                for (unsigned i = 0; i < Call->getNumArgs(); ++i) {
                    Expr *Arg = Call->getArg(i);
                    Arg->dump(); // 打印参数的 AST 表示
                }

                // IdentifierInfo &II = Context.Idents.get("printf");
                // FD->setDeclName(DeclarationName(&II));
                // QualType ParamType = Context.IntTy;
                // ParmVarDecl *Param = ParmVarDecl::Create(Context, FD, SourceLocation(), SourceLocation(),
                //                                         &Context.Idents.get("param"), ParamType, nullptr, SC_None, nullptr);
                // SmallVector<ParmVarDecl *, 1> Params;
                // Params.push_back(Param);
                // FD->setParams(Params);
                QualType CharPtrType = Context.getPointerType(Context.CharTy.withConst());
                // 创建新的字符串字面量参数
                StringLiteral *NewStrLiteral = StringLiteral::Create(Context, "Hello world!\n", StringLiteralKind::Ordinary, false, CharPtrType, SourceLocation());

                // 创建 ArrayToPointerDecay 转换
                ImplicitCastExpr *ArrayToPointerDecay = ImplicitCastExpr::Create(Context, CharPtrType, CK_ArrayToPointerDecay, NewStrLiteral, nullptr, VK_PRValue, FPOptionsOverride());

                // 创建 NoOp 转换
                ImplicitCastExpr *NoOpCast = ImplicitCastExpr::Create(Context, CharPtrType, CK_NoOp, ArrayToPointerDecay, nullptr, VK_PRValue, FPOptionsOverride());

                // 修改第一个参数为新的字符串字面量
                Call->setArg(0, NoOpCast);

                llvm::outs() << "After modify: Function Name: " << FD->getNameAsString() << "\n";
                for (unsigned i = 0; i < Call->getNumArgs(); ++i) {
                    Expr *Arg = Call->getArg(i);
                    Arg->dump(); // 打印参数的 AST 表示
                }
            }
        }
        return true;
    }

    // 在函数体中添加一个新的语句
    // bool VisitStmt(Stmt *S) {
    //     if (CompoundStmt *CS = dyn_cast<CompoundStmt>(S)) {
    //         ASTContext &Ctx = *Context;

            // 获取printf函数的声明
            // TranslationUnitDecl *TUDecl = Ctx.getTranslationUnitDecl();
            // FunctionDecl *PrintfDecl = nullptr;
            // for (auto *D : TUDecl->decls()) {
            //     if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
            //         if (FD->getName() == "printf") {
            //             PrintfDecl = FD;
            //             break;
            //         }
            //     }
            // }

            // if (!PrintfDecl) {
            //     llvm::errs() << "Error: printf function not found.\n";
            //     return false;
            // }

            // 创建printf函数调用的参数
            // StringLiteral *FormatStr = StringLiteral::Create(Ctx, "Hello, World!\n", StringLiteralKind::Ordinary, false, Ctx.CharTy, SourceLocation());
            // DeclRefExpr *PrintfRef = DeclRefExpr::Create(Ctx, NestedNameSpecifierLoc(), SourceLocation(), PrintfDecl, false, SourceLocation(), PrintfDecl->getType(), VK_LValue);

            // PrintfDecl->setArg(0, FormatStr);

            // 创建printf函数调用
            // Expr *PrintfArgs[] = { FormatStr };
            // ArrayRef<Expr *> Args(PrintfArgs);
            // CallExpr *PrintfCall = CallExpr::Create(Ctx, PrintfRef, PrintfArgs, Ctx.IntTy, ExprValueKind::VK_PRValue, SourceLocation(), FPOptionsOverride());

            // // 创建一个新的语句块并插入printf调用
            // SmallVector<Stmt*, 8> NewBody;
            // NewBody.push_back(PrintfCall);
            // NewBody.append(CS->body().begin(), CS->body().end());

            // CompoundStmt *NewCS = CompoundStmt::Create(Ctx, NewBody, FPOptionsOverride(), CS->getLBracLoc(), CS->getRBracLoc());

            // // 替换旧的CompoundStmt
            // // ReplaceStmt(CS, NewCS);
            // if (ParentMapContext *PM = Context->getParentMapContext()) {
            //     if (Stmt *Parent = PM->getParents(CS)) {
            //         if (CompoundStmt *ParentCS = dyn_cast<CompoundStmt>(Parent)) {
            //             for (auto &Child : ParentCS->body()) {
            //                 if (Child == CS) {
            //                     Child = NewCS;
            //                     break;
            //                 }
            //             }
            //         }
            //     }
            // }
        // }
        // return true;
    // }

private:
    ASTContext &Context;
};

//-----------------------------------------------------------------------------
// ASTConsumer
//-----------------------------------------------------------------------------
class CodeRefactorASTConsumer : public clang::ASTConsumer {
public:
    explicit CodeRefactorASTConsumer(ASTContext &Context) : Visitor(Context) {}

    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }

private:
    CodeRefactorASTVisitor Visitor;
};

//-----------------------------------------------------------------------------
// FrontendAction 定义触发插件动作
//-----------------------------------------------------------------------------
class CodeRefactorAddPluginAction : public PluginASTAction {
public:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                  StringRef file) override {
        llvm::outs() << "ASTMatcher ing \n" ;
        return std::make_unique<CodeRefactorASTConsumer>(CI.getASTContext());
    }

    bool ParseArgs(const CompilerInstance &CI,
                        const std::vector<std::string> &arg){
        return true;
    }
    static void PrintHelp(llvm::raw_ostream &ros) {
        ros << "Help for CodeRefactor plugin goes here\n";
    }
    ActionType getActionType() override {
        return AddBeforeMainAction;
    }
};

//-----------------------------------------------------------------------------
// Registration
//-----------------------------------------------------------------------------
static FrontendPluginRegistry::Add<CodeRefactorAddPluginAction>
X(/*Name=*/"CodeRefactor",
  /*Desc=*/"Change the name of a class method");
