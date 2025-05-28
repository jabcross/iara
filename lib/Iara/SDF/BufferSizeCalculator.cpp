#include "Iara/SDF/BufferSizeCalculator.h"
#include "Iara/Util/Range.h"

namespace iara::sdf {

llvm::FailureOr<BufferSizeValues>
calculateBufferSize(llvm::SmallVector<i64> &rates,
                    llvm::SmallVector<i64> &delays) {
  // WIP: Using slow script.
  // TODO: reimplement using presburger hermite normal form.

  assert(rates.size() == delays.size() + 1);

  Vec<i64> alpha{}, beta{};
  std::string command;
  llvm::raw_string_ostream ss(command);
  auto iara_dir = std::getenv("IARA_DIR");

  if (iara_dir == NULL or strnlen(iara_dir, 256) == 0) {
    llvm::errs()
        << "ERROR: Empty environment variable IARA_DIR. Did you source the "
           ".env file? \n";
    return llvm::failure();
  };

  llvm::errs() << "Value of IARA_DIR: >" << iara_dir << "< ";

  ss << "sh -c \"sh " << std::getenv("IARA_DIR")
     << "/scripts/calc-buf-size.sh ";
  for (size_t i = 0; i < delays.size(); i++) {
    ss << rates[i] << " " << delays[i] << " ";
  }
  ss << " " << rates.back();

  llvm::SmallString<128> errpath;
  auto error_code = llvm::sys::fs::createTemporaryFile("", ".txt", errpath);

  if (error_code.value() != 0) {
    llvm::errs() << "Error creating temporary file: " << errpath << "\n";
    return llvm::failure();
  }

  ss << " 2> " << errpath << "\"";

  llvm::errs() << "Running command: " << command << "\n";

  char buffer[128] = "";
  std::string output;
  auto pipe = popen(command.c_str(), "r");
  while (!feof(pipe)) {
    if (fgets(buffer, sizeof(buffer), pipe))
      output += buffer;
  }
  auto return_code = pclose(pipe);

  llvm::errs() << "Output: " << output << "\n";
  llvm::errs() << "Exit code: " << return_code << "\n";

  if (return_code != 0) {
    llvm::errs() << "Error in buffer size calculation (return code "
                 << return_code << ")";
    return llvm::failure();
  }

  auto error =
      llvm::MemoryBuffer::getFileAsStream(errpath)->get()->getBuffer().str();

  auto lines = llvm::split(output, "\n") | iara::util::range::IntoVector();

  {
    i64 val;
    while (!lines[0].consumeInteger(10, val)) {
      alpha.push_back(val);
      lines[0] = lines[0].drop_while([](char c) { return c == ' '; });
    }
    while (!lines[1].consumeInteger(10, val)) {
      beta.push_back(val);
      lines[1] = lines[1].drop_while([](char c) { return c == ' '; });
    }
  }
  return {{alpha, beta}};
}
} // namespace iara::sdf
