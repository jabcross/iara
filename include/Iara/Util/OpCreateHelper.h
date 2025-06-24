#ifndef UTIL_OPCREATEHELPER_H
#define UTIL_OPCREATEHELPER_H

#include <mlir/IR/Builders.h>
#include <mlir/IR/Location.h>
#include <source_location>

#ifdef IARA_DEBUG
  #define IARA_SET_DEBUG_LOC_ATTR(___debug_loc)                                \
    ___OP.getOperation()->setAttr("debug_loc",                                 \
                                  getDebugLoc(___BUILDER, ___debug_loc));
#else
  #define IARA_SET_DEBUG_LOC_ATTR
#endif

#ifdef IARA_DEBUG
  #define IARA_SET_DEBUG_LOC_ATTR_NAMED(___debug_loc, ___valuename)            \
    ___OP.getOperation()->setAttr(                                             \
        "debug_loc", getDebugLoc(___BUILDER, ___debug_loc, ___valuename));
#else
  #define IARA_SET_DEBUG_LOC_ATTR_NAMED
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

namespace {

template <class OpType> struct CreateOpAutocompleteHelper {
  using _OpType = OpType;
  inline static std::tuple<mlir::OpBuilder, mlir::Location>
  getBuilderAndLoc(mlir::OpBuilder builder, mlir::Location loc) {
    return {builder, loc};
  }
};

} // namespace

// Help with op autocompletion, inlay type hints and debugging.
// Creates an op and assigns it to the provided variable and type.
// Adds variable name to debug location.
// Parameters:
// - variable type
// - variable name
// - op type
// - builder
// - op location
// - YourOp::build() arguments
#define DEF_OP(___rvtype, ___valuename, ___optype, ___builder, ___loc, ...)    \
  [[maybe_unused]] auto ___valuename = [&](IARA_DEBUG_LOC_PARAM) {             \
    using ___HELPER = CreateOpAutocompleteHelper<___optype>;                   \
    using ___OPTYPE = ___HELPER::_OpType;                                      \
    auto [___BUILDER, ___LOC] =                                                \
        ___HELPER::getBuilderAndLoc(___builder, ___loc);                       \
    auto ___NAMED_LOC = ::llvm::cast<mlir::Location>(::mlir::NameLoc::get(     \
        ::mlir::StringAttr::get(___loc.getContext(), #___valuename), ___LOC)); \
    OperationState ___STATE{___NAMED_LOC, ___OPTYPE::getOperationName()};      \
    ___OPTYPE::build(___BUILDER, ___STATE, __VA_ARGS__);                       \
    ___OPTYPE ___OP = dyn_cast<___OPTYPE>(___BUILDER.create(___STATE));        \
    IARA_SET_DEBUG_LOC_ATTR_NAMED(debug_loc, #___valuename)                    \
    ___rvtype rv = ___OP;                                                      \
    return rv;                                                                 \
  }()

inline mlir::Attribute getDebugLoc(mlir::OpBuilder builder,
                                   std::source_location debug_loc,
                                   std::string value_name = "") {
  auto str = std::string();
  if (!value_name.empty()) {
    str += value_name;
    str += " @ ";
  }
  str += debug_loc.file_name();
  str += ":";
  str += std::to_string(debug_loc.line());
  str += ":";
  str += std::to_string(debug_loc.column());
  auto loc = builder.getStringAttr(str);
  return loc;
}

#endif
