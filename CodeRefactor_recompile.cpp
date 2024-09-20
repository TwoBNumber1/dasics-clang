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

std::vector<std::string> Args;
std::vector<std::vector<std::string>> Args_vec;
std::map<std::string, std::vector<std::vector<std::string>>> Bound_map;
std::string UFuncName;
bool is_recompile = false;
void BoundHandler::HandlePragma(clang::Preprocessor &PP, clang::PragmaIntroducer Introducer, clang::Token &FirstToken) {
  clang::SourceLocation Loc = FirstToken.getLocation();
  clang::SourceManager &SM = PP.getSourceManager();

  if(is_recompile){
    return;
  }

  /* 解析pragma后面紧跟的参数 以逗号区分不同参数 以括号为边界 嵌套括号整体视为一个参数 */
  clang::Token Tok;
  PP.Lex(Tok);

  if (Tok.isNot(clang::tok::l_paren)) {
    PP.getDiagnostics().Report(Loc, PP.getDiagnostics().getCustomDiagID(clang::DiagnosticsEngine::Error, "Expected '(' after #pragma bound"));
    return;
  }

  std::string CurrentArg;
  int parenDepth = 1;
  Args.clear();

  while (parenDepth > 0) {
    PP.Lex(Tok);
    if (Tok.is(clang::tok::r_paren)) {
      parenDepth--;
      if (parenDepth == 0) {
        if (!CurrentArg.empty()) {
          Args.push_back(CurrentArg);
        }
        break;
      } else {
        CurrentArg += ")";
        continue;
      }
    } else if (Tok.is(clang::tok::l_paren)) {
      parenDepth++;
      CurrentArg += "(";
      continue;
    } else if (Tok.is(clang::tok::comma)) {
      if(parenDepth == 1){
        Args.push_back(CurrentArg);
        CurrentArg.clear();
      } else if(parenDepth > 1){
        CurrentArg += ",";
      }
      continue;
    }

    if (Tok.is(clang::tok::identifier) || Tok.is(clang::tok::numeric_constant) || Tok.is(clang::tok::string_literal) || Tok.is(clang::tok::pipe)) {
      if (!CurrentArg.empty()) {
        CurrentArg += " ";
      }
      if (Tok.is(clang::tok::identifier)) {
        CurrentArg += Tok.getIdentifierInfo()->getName().str();
      } else if (Tok.is(clang::tok::pipe)) {
        CurrentArg += "|";
      } else {
        CurrentArg += std::string(Tok.getLiteralData(), Tok.getLength());
      }
    } else {
      PP.getDiagnostics().Report(Loc, PP.getDiagnostics().getCustomDiagID(clang::DiagnosticsEngine::Error, "Unexpected token in #pragma bound arguments"));
      return;
    }
    
  }

  if (Args.size() != 3) {
    PP.getDiagnostics().Report(Loc, PP.getDiagnostics().getCustomDiagID(clang::DiagnosticsEngine::Error, "Expected exactly three arguments in #pragma bound"));
    return;
  }
  Args_vec.push_back(Args);
  llvm::outs() << "Found #pragma bound with arguments: " << Args[0] << ", " << Args[1] << ", " << Args[2] << "\n";
}

void UntrustedCallHandler::HandlePragma(clang::Preprocessor &PP, clang::PragmaIntroducer Introducer, clang::Token &FirstToken) {
  /* 解析紧跟在 #pragma 后面的函数调用 使用一个自定义的lexer 避免影响预处理器 不过函数还是需要去符号表查一下 */
  clang::Token Tok;
  clang::SourceLocation Loc = FirstToken.getLocation();

  if(is_recompile){
    return;
  }

  PP.Lex(Tok);
  while(Tok.isNot(tok::eod)) {
    PP.Lex(Tok);
  }
  clang::SourceLocation FuncLoc = Tok.getLocation();
  clang::Lexer Lexer(FuncLoc, PP.getLangOpts(), PP.getSourceManager().getCharacterData(FuncLoc), PP.getSourceManager().getCharacterData(FuncLoc), PP.getSourceManager().getCharacterData(FuncLoc) + 10);
  Lexer.SetKeepWhitespaceMode(true);
  Lexer.LexFromRawLexer(Tok); /* skip eod */
  while(Tok.is(tok::unknown)) {
    Lexer.LexFromRawLexer(Tok);
  }
  if (Tok.is(clang::tok::raw_identifier)) {
    PP.LookUpIdentifierInfo(Tok);
  }
  if (Tok.is(clang::tok::identifier)) {
    clang::Token Func_token = Tok;
    Lexer.LexFromRawLexer(Tok);
    if (Tok.is(clang::tok::l_paren)) {
      UFuncName = Func_token.getIdentifierInfo()->getName().str();
      llvm::outs() << "Found function call: " << UFuncName << "\n";
      Bound_map[UFuncName] = Args_vec;
      llvm::outs() << "With bound: \n";
      for(auto &arg : Args_vec){
        for(auto &arg_item : arg){
          llvm::outs() << arg_item << " ";
        }
        llvm::outs() << "\n";
      }
    } else {
      PP.getDiagnostics().Report(Loc, PP.getDiagnostics().getCustomDiagID(clang::DiagnosticsEngine::Error, "Expected '(' after function name"));
    }
  } else {
    PP.getDiagnostics().Report(Loc, PP.getDiagnostics().getCustomDiagID(clang::DiagnosticsEngine::Error, "Expected function name after #pragma bound"));
  }
  
}

class FireMatchCallback : public clang::ast_matchers::MatchFinder::MatchCallback
{
public:
  FireMatchCallback(clang::ASTContext &Context,
                    clang::FileID *FileID,
                    clang::Rewriter *FileRewriter
                   )
  : Context_(Context),
    FileID_(FileID),
    FileRewriter_(FileRewriter)
  {}

  void run(clang::ast_matchers::MatchFinder::MatchResult const &Result) override{
    //在Consumer中bind过的name 传递给getNodeAs可以找到该处节点
    llvm::outs() << "ASTMatcher occur.\n" ;
    const CallExpr *Call = Result.Nodes.getNodeAs<clang::CallExpr>(UFuncName);
    const SourceManager &SM = Context_.getSourceManager();
    //表达式位置
    SourceLocation CallLoc = Call->getExprLoc();
    // 获取函数调用的 SourceRange
    SourceRange Range = Call->getSourceRange();

    // 创建新的函数调用字符串
    std::string BoudCode = "" + Args[0] + "_handler = dasics_libcfg_alloc(" \
      + Args[2] + ", (uint64_t)" + Args[0] + ", (uint64_t)" + Args[0] + " + " + Args[1] + " - 1);\n\t";
    std::string SPHandeCode = std::string("") + "uint64_t sp;\n\t" + "asm volatile (\"mv %0, sp\" : \"=r\"(sp));\n\t" \
      + "stack_handler = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, sp - 0x2000, sp);\n\t";
    std::string FreeCode = std::string("") + "dasics_libcfg_free(" + Args[0] + "_handler);\n\t" + "dasics_libcfg_free(stack_handler);\n\t";
    std::string CallCode = std::string("") + "lib_call(&" + UFuncName + ", (uint64_t)" + Args[0] + ");\n\t";

    //找到主函数 -> FileID
    FunctionDecl *Main = nullptr;
    TranslationUnitDecl *TU = Context_.getTranslationUnitDecl();
    for (auto *D : TU->decls()) {
        if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
            if (FD->getNameInfo().getName().getAsString() == "main") {
                Main = FD;
            }
        }
    }
    // 在函数调用之前插入新的代码
    llvm::outs() << "Rewrite text.. ..\n"; 

    // 获取函数调用的结束位置
    SourceLocation EndLoc = Call->getEndLoc();
    // 移动到分号之后的位置
    while (true) {
      EndLoc = EndLoc.getLocWithOffset(1);
      if (SM.getCharacterData(EndLoc)[0] == ';') {
          EndLoc = EndLoc.getLocWithOffset(1);
          break;
      }
    }
    FileRewriter_->ReplaceText(SourceRange(CallLoc, EndLoc), CallCode);
    FileRewriter_->InsertText(CallLoc , BoudCode, true);
    FileRewriter_->InsertText(CallLoc , SPHandeCode, true);
    FileRewriter_->InsertText(EndLoc, FreeCode, true);
    //FileRewriter_->InsertText(EndLoc, "printf(\"New\");", true);

    *FileID_ = SM.getFileID(Main->getBeginLoc());

  }
  
void onEndOfTranslationUnit() override{
  // Output to stdout 将修改过的代码打印到标准输出
  llvm::outs() << "onEndOfTranslationUnit\n";
  // if(FileRewriter_){llvm::outs() << "onEndOfTranslationUnit11\n";}
  
  // FileRewriter_->getEditBuffer(FileRewriter_->getSourceMgr().getMainFileID()).write(llvm::outs());
  // llvm::outs() << "onEndOfTranslationUnit12\n";
}

private:
  clang::ASTContext &Context_;
  clang::FileID *FileID_;
  clang::Rewriter *FileRewriter_;
};

class CallExprVisitor : public RecursiveASTVisitor<CallExprVisitor> {
public:
  //Debug用 中途取不到ASTContext
  // 重载 VisitCallExpr 函数
  bool VisitCallExpr(CallExpr *Call) {
    if (FunctionDecl *Callee = Call->getDirectCallee()) {
      llvm::outs() << "Function call: " << Callee->getNameInfo().getName().getAsString() << "\n";
    }
    return true; // 返回 true 继续遍历其他结点
  }

private:
  ASTContext *Context;
};


class FireConsumer : public clang::ASTConsumer
{
public:
  FireConsumer(clang::FileID *FileID,
               clang::Rewriter *FileRewriter,
               bool *FileRewriteError)
  : FileID_(FileID),
    FileRewriter_(FileRewriter),
    FileRewriteError_(FileRewriteError) {}

  void HandleTranslationUnit(clang::ASTContext &Context) override
  {
    using namespace clang::ast_matchers;
    /*
    llvm::outs() << "traversing. \n" ;
    CallExprVisitor Visitor;
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    */
    llvm::outs()  << "add Matcher...\n";
     // TODO:匹配 or 查找
    const auto MatcherForFunc = callExpr(callee(functionDecl(hasName(UFuncName)))).bind(UFuncName);
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

    llvm::outs() << FileName_ << " Create Consumer ... \n";
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

  // Automatically run the plugin after the main AST action
  // PluginASTAction::ActionType getActionType() override {
  //   llvm::outs() << "ActionType ... \n";
  //   return AddAfterMainAction;
  // }

  void EndSourceFileAction() override{
     auto FileRewriteBuffer { FileRewriter_.getRewriteBufferFor(FileID_) };
     llvm::outs() << "EndSourceFileAction ing\n";
    //  llvm::outs()<< UFuncName << "\n";
    if (!FileRewriteBuffer) {
        llvm::outs() << "Rewrite buffer is null, no modifications to apply.\n";
        //return;
    }else{
      std::string BufferContent = std::string(FileRewriteBuffer->begin(), FileRewriteBuffer->end());
      llvm::outs() << "Recompile Modified Code:\n" << BufferContent << "\n";
      is_recompile = true;
      compile(CI_, FileName_, FileRewriteBuffer->begin(), FileRewriteBuffer->end());
    }

}

private:
  clang::CompilerInstance *CI_;

  std::string FileName_;
  clang::FileID FileID_;
  clang::Rewriter FileRewriter_;
  bool FileRewriteError_ = false;
};

static clang::FrontendPluginRegistry::Add<FireAction> X("CodeRefactor", "generate code by recompile.");
static PragmaHandlerRegistry::Add<BoundHandler> P("bound", "");
static PragmaHandlerRegistry::Add<UntrustedCallHandler> P2("untrusted_call", "");
