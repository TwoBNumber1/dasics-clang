#include <memory>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>


#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/FixItRewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Tooling/Refactoring/Rename/RenamingAction.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Parse/ParseAST.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/FormatVariadic.h"

#include <string>
#include "Recompile.hpp"
using namespace clang;

namespace {


class FireMatchCallback : public clang::ast_matchers::MatchFinder::MatchCallback
{
public:
  FireMatchCallback(clang::ASTContext &Context,
                    clang::FileID *FileID,
                    clang::Rewriter *FileRewriter)
  : Context_(Context),
    FileID_(FileID),
    FileRewriter_(FileRewriter)
  {}

  void run(clang::ast_matchers::MatchFinder::MatchResult const &Result) override{

    //在Consumer中bind过的name 传递给getNodeAs可以找到该处节点
    llvm::outs() << "ASTMatcher occur.\n" ;
    const CallExpr *Call = Result.Nodes.getNodeAs<clang::CallExpr>("A");
    const ASTContext *Ctx = Result.Context;
    const SourceManager &SM = Ctx->getSourceManager();
    //表达式位置
    SourceLocation CallLoc = Call->getExprLoc();
    // 获取函数调用的 SourceRange
    SourceRange Range = Call->getSourceRange();

    // 创建新的函数调用字符串
    std::string NewCode = "printf(\"New\\n\");\n";

    //找到主函数 （FileID
    FunctionDecl *Main = nullptr;
    TranslationUnitDecl *TU = Ctx->getTranslationUnitDecl();
    for (auto *D : TU->decls()) {
        if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
            if (FD->getNameInfo().getName().getAsString() == "main") {
                Main = FD;
            }
        }
    }
    // 在函数调用之前插入新的代码
    llvm::outs() << "Rewrite text.. ..\n"; 
    FileRewriter_->InsertText(CallLoc , NewCode, true);
    *FileID_ = SM.getFileID(Main->getBeginLoc());
    // Obtain file ID of main function.

    auto &SourceManager { Context_.getSourceManager() };
    
    //从主函数获取文件id
    *FileID_ = SourceManager.getFileID(Main->getBeginLoc());
  }
  
   void onEndOfTranslationUnit() override{
  // Output to stdout 将修改过的代码打印到标准输出
  FileRewriter_->getEditBuffer(FileRewriter_->getSourceMgr().getMainFileID()).write(llvm::outs());
}

private:
  clang::ASTContext &Context_;
  clang::FileID *FileID_;
  clang::Rewriter *FileRewriter_;
};

class FireConsumer : public clang::ASTConsumer
{
public:
  FireConsumer(clang::FileID *FileID,
               clang::Rewriter *FileRewriter,
               bool *FileRewriteError)
  : FileID_(FileID),
    FileRewriter_(FileRewriter),
    FileRewriteError_(FileRewriteError)
  {}

  void HandleTranslationUnit(clang::ASTContext &Context) override
  {
    using namespace clang::ast_matchers;
    llvm::outs()  << "add Matcher...\n";
     // TODO:匹配 or 查找
    const auto MatcherForFunc = callExpr(callee(functionDecl(hasName("A")))).bind("A");

    // create match callback
    FireMatchCallback MatchCallback(Context, FileID_, FileRewriter_);

    // create and run match finder
    clang::ast_matchers::MatchFinder MatchFinder;
    MatchFinder.addMatcher(MatcherForFunc, &MatchCallback);
    MatchFinder.matchAST(Context);
  }


private:
  clang::FileID *FileID_;
  clang::Rewriter *FileRewriter_;
  bool *FileRewriteError_;
};

class FireAction : public clang::PluginASTAction
{
protected:
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
    clang::CompilerInstance &CI,
    llvm::StringRef FileName) override
  {
    auto &SourceManager { CI.getSourceManager() };
    auto &LangOpts { CI.getLangOpts() };

    CI_ = &CI;

    FileName_ = FileName.str();

    FileRewriter_.setSourceMgr(SourceManager, LangOpts);

    return std::make_unique<FireConsumer>(
        &FileID_, &FileRewriter_, &FileRewriteError_);
  }

  bool ParseArgs(clang::CompilerInstance const &,
                 std::vector<std::string> const &) override
  {
    return true;
  }

  clang::PluginASTAction::ActionType getActionType() override
  {
    return clang::PluginASTAction::ReplaceAction;
  }

  void EndSourceFileAction() override{
    auto FileRewriteBuffer { FileRewriter_.getRewriteBufferFor(FileID_) };
    std::string BufferContent = std::string(FileRewriteBuffer->begin(), FileRewriteBuffer->end());
    llvm::outs() << "Recompile Modified Code:\n" << BufferContent << "\n";
    compile(CI_, FileName_, FileRewriteBuffer->begin(), FileRewriteBuffer->end());
  }

private:
  clang::CompilerInstance *CI_;

  std::string FileName_;
  clang::FileID FileID_;
  clang::Rewriter FileRewriter_;
  bool FileRewriteError_ = false;
};

} // end namespace

static clang::FrontendPluginRegistry::Add<FireAction>
X("CodeRefactor", "generate code by recompile.");