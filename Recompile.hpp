#pragma once

#include <cassert>
#include <memory>
#include <string>

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "llvm/Support/MemoryBuffer.h"

template<typename IT>
inline void compile(clang::CompilerInstance *CI,
                    std::string const &FileName,
                    IT FileBegin,
                    IT FileEnd)
{
  auto &CodeGenOpts { CI->getCodeGenOpts() };
  auto &Target { CI->getTarget() };
  auto &Diagnostics { CI->getDiagnostics() };

  // create new compiler instance
  auto CInvNew { std::make_shared<clang::CompilerInvocation>() };

  // 将 std::vector<std::string> 转换为 std::vector<const char*> 不然参数不匹配
  std::vector<const char*> Args;
  for (const auto &Arg : CodeGenOpts.CommandLineArgs) {
      Args.push_back(Arg.c_str());
  }
  bool CInvNewCreated {
    clang::CompilerInvocation::CreateFromArgs(
      *CInvNew, llvm::ArrayRef<const char*>(Args), Diagnostics) };

  assert(CInvNewCreated);

  clang::CompilerInstance CINew;
  CINew.setInvocation(CInvNew);
  CINew.setTarget(&Target);
  CINew.createDiagnostics();

  // create rewrite buffer
  std::string FileContent { FileBegin, FileEnd };
  auto FileMemoryBuffer { llvm::MemoryBuffer::getMemBufferCopy(FileContent) };

  // create "virtual" input file 
  auto &PreprocessorOpts { CINew.getPreprocessorOpts() };
  PreprocessorOpts.addRemappedFile(FileName, FileMemoryBuffer.get());
  // generate code
  clang::EmitObjAction EmitObj;
  CINew.ExecuteAction(EmitObj);
  // clean up rewrite buffer  产生修改后的代码，并且不改变源文件
  FileMemoryBuffer.release();
}