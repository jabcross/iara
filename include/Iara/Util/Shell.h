#ifndef UTIL_SHELL_H
#define UTIL_SHELL_H

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <sstream>
#include <system_error>
namespace shell {
struct ShellCallResult {
  std::error_code error_code;
  std::unique_ptr<llvm::MemoryBuffer> stdout;
  std::unique_ptr<llvm::MemoryBuffer> stderr;
};

inline ShellCallResult shellCall(std::string command) {

  std::string escaped_command = "sh -c \" ";
  escaped_command += command;

  llvm::SmallString<128> errpath;
  auto error_code = llvm::sys::fs::createTemporaryFile("", ".txt", errpath);
  if (error_code.value() != 0) {
    llvm::errs() << "Error creating temporary file: " << errpath << "\n";
    return {.error_code = error_code};
  }

  llvm::SmallString<128> outpath;
  error_code = llvm::sys::fs::createTemporaryFile("", ".txt", outpath);
  if (error_code.value() != 0) {
    llvm::errs() << "Error creating temporary file: " << outpath << "\n";
    return {.error_code = error_code};
  }

  auto ss = std::stringstream(escaped_command);

  ss << " 1> " << outpath.c_str() << " 2> " << errpath.c_str() << "\"";

  llvm::errs() << "Running command: " << escaped_command << "\n";

  char buffer[128] = "";
  auto pipe = popen(escaped_command.c_str(), "r");
  while (!feof(pipe)) {
    fgets(buffer, sizeof(buffer), pipe);
  }
  auto return_code = pclose(pipe);

  auto outstream =
      std::move(llvm::MemoryBuffer::getFileAsStream(outpath).get());
  auto errstream =
      std::move(llvm::MemoryBuffer::getFileAsStream(errpath).get());

  return {.error_code = std::make_error_code((std::errc)return_code),
          .stdout = std::move(outstream),
          .stderr = std::move(errstream)};
};

} // namespace shell
#endif
