#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/TokenKinds.h"
#include "clang/Lex/Token.h"
#include "clang/Lex/Pragma.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/raw_ostream.h"

#include "MacroGuardValidator.h"


using namespace clang;

namespace macro_guard {
    llvm::SmallVector<const IdentifierInfo*, 2> ArgsToEnclosed;
} // end namespace macro_guard

using namespace macro_guard;



namespace {
class MacroGuardPragma : public PragmaHandler {
  bool IsValidatorRegistered;

public:
  //#pragma name
  MacroGuardPragma() : PragmaHandler("bound"),
                       IsValidatorRegistered(false) {}

  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &PragmaTok) override;
};
} // end anonymous namespace
 
void MacroGuardPragma::HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                                    Token &PragmaTok) {
  // Reset the to-be-enclosed argument list
  ArgsToEnclosed.clear();

  //检查自定义#pragma后的所有参数
  Token Tok;
  PP.Lex(Tok);
  llvm::outs() << "#pragma Bound location: " << Introducer.Loc.printToString(PP.getSourceManager()) << "\n";
  while (Tok.isNot(tok::eod)) {
    // 获取Token的类型
    // clang::tok::TokenKind Kind = Tok.getKind();
    //TODO: 格式判断 
    llvm::outs() << "Token iteration:" << Tok.getName() << "\n";
    if (auto *II = Tok.getIdentifierInfo()) {
      ArgsToEnclosed.push_back(II);

    }
    PP.Lex(Tok);
  }

  if (!IsValidatorRegistered) {
    // Register the validator PPCallbacks
    auto Validator = std::make_unique<MacroGuardValidator>(PP.getSourceManager());
    PP.addPPCallbacks(std::move(Validator));
    IsValidatorRegistered = true;
  }
}
/*  声明了这个变量之后，当这个插件加载到 clang 时，一个 MacroGuardHandler 实例将插入到全局的 PragmaHandler 注册表中，
    当它遇到一个非标准的 #pragma 指令时，预处理器将查询注册表
*/
static PragmaHandlerRegistry::Add<MacroGuardPragma> X("bound", "");