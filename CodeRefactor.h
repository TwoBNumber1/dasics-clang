//==============================================================================
// FILE:
//    CodeRefactor.h
//
// DESCRIPTION:
//
// License: The Unlicense
//==============================================================================
#ifndef CLANG_TUTOR_CODEREFACTOR_H
#define CLANG_TUTOR_CODEREFACTOR_H
#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/FixItRewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Frontend/FrontendActions.h"

//extern clang::CompilerInstance *g_CI;
//-----------------------------------------------------------------------------
// ASTFinder callback
//-----------------------------------------------------------------------------
class CodeRefactorMatcher
    : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
  explicit CodeRefactorMatcher(clang::Rewriter &RewriterForCodeRefactor)
      : CodeRefactorRewriter(RewriterForCodeRefactor) {}
  void onEndOfTranslationUnit() override;

  void run(const clang::ast_matchers::MatchFinder::MatchResult &) override;

private:
  clang::Rewriter CodeRefactorRewriter;

};

//-----------------------------------------------------------------------------
// ASTConsumer
//-----------------------------------------------------------------------------
class CodeRefactorASTConsumer : public clang::ASTConsumer {
  
public:
  CodeRefactorASTConsumer(clang::Rewriter &R);
  void HandleTranslationUnit(clang::ASTContext &Ctx) override {
    Finder.matchAST(Ctx);
    //打印修改后的AST
    Ctx.getTranslationUnitDecl()->dump();  
  }

private:
  clang::ast_matchers::MatchFinder Finder;
  CodeRefactorMatcher CodeRefactorHandler;

};

#endif
