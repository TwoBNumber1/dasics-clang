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

//代码检查回调 Matcher == handler
void CodeRefactorMatcher::run(const MatchFinder::MatchResult &Result) {

  //在Consumer中bind过的name 传递给getNodeAs可以找到该处节点
  const MemberExpr *MemberAccess =
      Result.Nodes.getNodeAs<clang::MemberExpr>("MemberAccess");

  const NamedDecl *MemberDecl =
      Result.Nodes.getNodeAs<clang::NamedDecl>("MemberDecl");

  //TODO:创建节点并插入... 
 
}

void CodeRefactorMatcher::onEndOfTranslationUnit() {
  // Output to stdout 将修改过的代码打印到标准输出
  CodeRefactorRewriter
      .getEditBuffer(CodeRefactorRewriter.getSourceMgr().getMainFileID())
      .write(llvm::outs());
}

// 设置Matcher的匹配规则，匹配成功的AST结点交给handler参数进行处理
CodeRefactorASTConsumer::CodeRefactorASTConsumer(Rewriter &R)
    : CodeRefactorHandler(R) {

  // TODO:匹配 or 查找    
  // const auto MatcherForMemberAccess = cxxMemberCallExpr(
  //     callee(memberExpr(member(hasName(OldName))).bind("MemberAccess")),
  //     thisPointerType(cxxRecordDecl(isSameOrDerivedFrom(hasName(ClassName)))));

  // Finder.addMatcher(MatcherForMemberAccess, &CodeRefactorHandler);

  // const auto MatcherForMemberDecl = cxxRecordDecl(
  //     allOf(isSameOrDerivedFrom(hasName(ClassName)),
  //           hasMethod(decl(namedDecl(hasName(OldName))).bind("MemberDecl"))));

  // Finder.addMatcher(MatcherForMemberDecl, &CodeRefactorHandler);
}

//-----------------------------------------------------------------------------
// FrontendAction 定义触发插件动作
//-----------------------------------------------------------------------------
class CodeRefactorAddPluginAction : public PluginASTAction {
public:
   bool ParseArgs(const CompilerInstance &CI,
                       const std::vector<std::string> &arg){
            return true;
  }

  static void PrintHelp(llvm::raw_ostream &ros) {
    ros << "Help for CodeRefactor plugin goes here\n";
  }

  // Returns our ASTConsumer per translation unit.
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override {
    //设置触发时传递给translantion unit的参数 此处传递rewriter
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
