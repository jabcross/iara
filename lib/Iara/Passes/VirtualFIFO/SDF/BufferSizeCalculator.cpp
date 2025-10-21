#include "Iara/Passes/VirtualFIFO/SDF/BufferSizeCalculator.h"
#include "Iara/Util/Range.h"
#include <llvm/ADT/Hashing.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/ErrorHandling.h>

namespace iara::passes::virtualfifo::sdf {

std::pair<bool, BufferSizeValues *>
BufferSizeMemo::get(llvm::SmallVector<i64> &rates,
                    llvm::SmallVector<i64> &delays) {
  llvm::hash_code code = llvm::hash_combine(
      llvm::hash_combine_range(rates.begin(), rates.end()),
      llvm::hash_combine_range(delays.begin(), delays.end()));

  auto pair = memo.find(code);

  if (pair == memo.end()) {
    return {false, &memo[code]};
  }

  return {true, &pair->second};
}

llvm::FailureOr<BufferSizeValues *>
calculateBufferSize(BufferSizeMemo &memo,
                    llvm::SmallVector<i64> &rates,
                    llvm::SmallVector<i64> &delays) {
  // WIP: Using slow script.
  // TODO: reimplement using presburger hermite normal form.

  auto [existing, values] = memo.get(rates, delays);

  if (existing) {
    return {values};
  }

  assert(rates.size() == delays.size() + 1);

  if (rates.size() == 2 && rates[0] == rates[1] && delays[0] == 0) {
    *values = BufferSizeValues{{1, 1}, {1, 1}};
    return values;
  }

  for (auto delay : delays) {
    if (delay < 0) {
      llvm_unreachable("invalid delay size");
    }
  }

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
    while (!lines[2].consumeInteger(10, val)) {
      values->alpha.push_back(val);
      lines[2] = lines[2].drop_while([](char c) { return c == ' '; });
    }
    while (!lines[3].consumeInteger(10, val)) {
      values->beta.push_back(val);
      lines[3] = lines[3].drop_while([](char c) { return c == ' '; });
    }
  }

  return {values};
}
} // namespace iara::passes::virtualfifo::sdf
