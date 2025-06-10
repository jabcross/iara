#ifndef UTIL_OPCREATEHELPER_H
#define UTIL_OPCREATEHELPER_H

#include <mlir/IR/Builders.h>
#include <source_location>

#ifdef IARA_DEBUG
  #define IARA_SET_DEBUG_LOC_ATTR(___debug_loc)                                \
    ___OP.getOperation()->setAttr("debug_loc",                                 \
                                  getDebugLoc(___BUILDER, ___debug_loc));
#else
  #define IARA_SET_DEBUG_LOC_ATTR
#endif

#ifdef IARA_DEBUG
  #define IARA_DEBUG_LOC_PARAM                                                 \
    std::source_location debug_loc = std::source_location::current()
#else
  #define IARA_DEBUG_LOC_PARAM
#endif

// Help with op autocompletion, inlay type hints and debugging.
// Parameters:
// - op type
// - builder
// - op location
// - YourOp::build() arguments
#define CREATE(___type, ___builder, ___loc, ...)                               \
  [&](IARA_DEBUG_LOC_PARAM) -> ___type {                                       \
    using ___TYPE = ___type;                                                   \
    OperationState ___STATE{___loc, ___TYPE::getOperationName()};              \
    OpBuilder ___BUILDER = ___builder;                                         \
    ___TYPE::build(___BUILDER, ___STATE, __VA_ARGS__);                         \
    ___TYPE ___OP = dyn_cast<___TYPE>(___BUILDER.create(___STATE));            \
    IARA_SET_DEBUG_LOC_ATTR(debug_loc)                                         \
    return ___OP;                                                              \
  }()

#define DEF_OP(___rvtype, ___name, ___optype, ___builder, ___loc, ...)         \
  ___rvtype ___name = CREATE(                                                  \
      ___optype,                                                               \
      ___builder,                                                              \
      ::llvm::cast<Location>(::mlir::NameLoc::get(                             \
          ::mlir::StringAttr::get(___loc.getContext(), #___name), ___loc)),    \
      __VA_ARGS__)

inline mlir::Attribute getDebugLoc(mlir::OpBuilder builder,
                                   std::source_location debug_loc) {
  auto str = std::string();
  str += debug_loc.file_name();
  str += ":";
  str += std::to_string(debug_loc.line());
  str += ":";
  str += std::to_string(debug_loc.column());
  auto loc = builder.getStringAttr(str);
  return loc;
}

#endif
