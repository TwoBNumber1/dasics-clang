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

// 非强制要求包含头文件
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Type.h"
#include "clang/Basic/IdentifierTable.h"

using namespace clang;
using namespace ast_matchers;

// other auxiliary function call
FunctionDecl *createPrintfDecl(ASTContext &Context) {
    // 获取 printf 函数的返回类型：int
    QualType ReturnType = Context.IntTy;

    // 构建 printf 函数的参数列表：const char * 和 ...
    QualType CharPtrType = Context.getPointerType(Context.CharTy);
    ASTContext::GetBuiltinTypeError Error;

    QualType EllipsisType = Context.GetBuiltinType(BuiltinType::Void, Error, nullptr);
    
    // 参数类型列表
    SmallVector<QualType, 1> ParamTypes;
    ParamTypes.push_back(CharPtrType);

    // 创建参数声明
    SmallVector<ParmVarDecl *, 1> Params;
    Params.push_back(ParmVarDecl::Create(Context, nullptr, SourceLocation(), SourceLocation(),
                                         &Context.Idents.get("format"), CharPtrType, nullptr,
                                         SC_None, nullptr));

    // 创建函数声明
    FunctionProtoType::ExtProtoInfo EPI;
    EPI.Variadic = true;
    QualType FuncType = Context.getFunctionType(ReturnType, ParamTypes, EPI);

    FunctionDecl *PrintfDecl = FunctionDecl::Create(Context, Context.getTranslationUnitDecl(),
                                                    SourceLocation(), SourceLocation(),
                                                    &Context.Idents.get("printf"), FuncType,
                                                    nullptr, SC_Extern);
    PrintfDecl->setParams(Params);
    return PrintfDecl;
}

Stmt *createPrintfCall(ASTContext &Context, SourceRange Range) {

    QualType StrTy = Context.CharTy.withConst();
    // 创建函数名的字符串字面量 "New"
    SourceManager &SM = Context.getSourceManager();
    SourceLocation NewLoc = SM.getExpansionLoc(Range.getBegin());
    StringLiteral *Literal = StringLiteral::Create(
        Context, "New.\n",  StringLiteralKind::Ordinary, false,
        StrTy/*Context.CharTy*/, NewLoc);

    // 创建 printf 函数声明（假设已存在）
    FunctionDecl *PrintfDecl = createPrintfDecl(Context);

    // 创建 printf 参数列表
    std::vector<Expr*> Args;
    Args.push_back(Literal);

    // 创建 DeclRefExpr 表示 printf 函数
    DeclRefExpr *PrintfRef = DeclRefExpr::Create(
        Context, NestedNameSpecifierLoc(), SourceLocation(),
        PrintfDecl, false, NewLoc, PrintfDecl->getType(), ExprValueKind::VK_PRValue);

    // 创建 printf 调用表达式
    CallExpr *PrintfCall = CallExpr::Create(
        Context, PrintfRef, Args, PrintfDecl->getReturnType(), ExprValueKind::VK_PRValue, SourceLocation(),
                                                        FPOptionsOverride(), Args.size(), CallExpr::ADLCallKind::NotADL);

    // 将 CallExpr 包装为一个语句返回
    return PrintfCall;
}

//-----------------------------------------------------------------------------
// CodeRefactorMatcher - implementation
//-----------------------------------------------------------------------------

//代码检查回调 Matcher == handler match触发
void CodeRefactorMatcher::run(const MatchFinder::MatchResult &Result) {

  //在Consumer中bind过的name 传递给getNodeAs可以找到该处节点
  llvm::outs() << "ASTMatcher occur.\n" ;
  const CallExpr *Call = Result.Nodes.getNodeAs<clang::CallExpr>("A");
  ASTContext *Ctx = Result.Context;
  SourceManager &SM = Ctx->getSourceManager();

  // 克隆 CallExpr 节点
  llvm::SmallVector<Expr*, 4> Args;
  for (const Expr *Arg : Call->arguments()) {
      Args.push_back(const_cast<Expr*>(Arg));
  }

  // 修改参数，例如替换为新的字符串常量
  auto *StringLiteral = Ctx->getStringLiteral("New Argument");
  Args[0] = StringLiteral;  // 修改第一个参数

  // 创建新的 CallExpr 节点
  CallExpr *NewCall = CallExpr::Create(
      Ctx,Call->getCallee(), Args, Call->getType(),
      Call->getValueKind(), Call->getRParenLoc());


  //表达式位置
  SourceLocation CallLoc = Call->getExprLoc();
  //获取函数调用的 SourceRange
  SourceRange Range = Call->getSourceRange();

  //TODO:插入AST... 
  TranslationUnitDecl *TU = Ctx->getTranslationUnitDecl();
  for (auto *D : TU->decls()) {
      if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
          if (FD->getNameInfo().getName().getAsString() == "main") {
              
              if (CompoundStmt *Body = dyn_cast<CompoundStmt>(FD->getBody())) {
                //创建一个新的语句
                  Stmt *NewStmt = NewCall;
                  // 创建一个新的语句列表并插入 printf 调用
                  std::vector<Stmt *> NewStmtsVec;
                  NewStmtsVec.push_back(NewStmt);
                  //将原本的逐个
                  for (Stmt *S : Body->body()) {
                      //TODO:判断名称插入statement
                      NewStmtsVec.push_back(S);
                  }

                  llvm::ArrayRef<Stmt *> NewStmts(NewStmtsVec);
                    // 获取或初始化 FPOptionsOverride
                    FPOptionsOverride FPFeatures = Body->hasStoredFPFeatures()
                        ? Body->getStoredFPFeatures()
                        : FPOptionsOverride();
                  // 创建一个新的 CompoundStmt，包含新的语句列表
                  CompoundStmt *NewCompoundStmt = CompoundStmt::Create(
                      *Ctx, NewStmts, FPFeatures, Body->getLBracLoc(),
                      Body->getRBracLoc());

                  // 将新的CompoundStmt 设置为函数体
                  FD->setBody(NewCompoundStmt);
              }
          }
      }
  }

}

void CodeRefactorMatcher::onEndOfTranslationUnit() {

  // 我估计没有用啊这行代码
  // 草，真的有用 下班
  // 仍然没用 去你MD
  CodeRefactorRewriter.overwriteChangedFiles();
  // Output to stdout 将修改过的代码打印到标准输出
  CodeRefactorRewriter
      .getEditBuffer(CodeRefactorRewriter.getSourceMgr().getMainFileID())
      .write(llvm::outs());
  
}

// 设置Matcher的匹配规则，匹配成功的AST结点交给handler参数进行处理
CodeRefactorASTConsumer::CodeRefactorASTConsumer(Rewriter &R)
    : CodeRefactorHandler(R) {
  llvm::outs() << "ASTMatcher ing \n" ;

  // TODO:匹配 or 查找
  // DO个P 天天想死
    const auto MatcherForFunc = callExpr(
       callee(functionDecl(hasName("A")))).bind("A");
  // const auto MatcherForMemberAccess = cxxMemberCallExpr(
  //     callee(memberExpr(member(hasName(OldName))).bind("MemberAccess")),
  //     thisPointerType(cxxRecordDecl(isSameOrDerivedFrom(hasName(ClassName)))));

  // Finder.addMatcher(MatcherForMemberAccess, &CodeRefactorHandler);


    Finder.addMatcher(MatcherForFunc, &CodeRefactorHandler);
}

//-----------------------------------------------------------------------------
// FrontendAction 定义触发插件动作
//-----------------------------------------------------------------------------
class CodeRefactorAddPluginAction : public PluginASTAction {
public:
   bool ParseArgs(const CompilerInstance &CI,
                       const std::vector<std::string> &arg){
                        llvm::errs() << "Plugin ParseArgs Loaded\n";
            return true;
  }

  static void PrintHelp(llvm::raw_ostream &ros) {
    ros << "Help for CodeRefactor plugin goes here\n";
  }

  // Returns our ASTConsumer per translation unit.
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override {
    //设置触发时传递给translantion unit的参数 此处传递rewriter
    llvm::outs() << "ASTMatcher ing \n" ;
    llvm::errs() << "Plugin Loaded\n";
    RewriterForCodeRefactor.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<CodeRefactorASTConsumer>(RewriterForCodeRefactor);
  }

private:
  Rewriter RewriterForCodeRefactor;
};

//-----------------------------------------------------------------------------
// Registration
//-----------------------------------------------------------------------------
static FrontendPluginRegistry::Add<CodeRefactorAddPluginAction>
    X(/*Name=*/"CodeRefactor",
      /*Desc=*/"Change the name of a class method");
