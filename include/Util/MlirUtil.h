#include <mlir/Dialect/Func/IR/FuncOps.h>

namespace mlir {
func::FuncOp createEmptyVoidFunctionWithBody(OpBuilder builder, StringRef name,
                                             Location loc);
OpOperand &appendOperand(Operation *op, Value val);
void moveBlockAfter(Block *to_move, Block *after_this);

} // namespace mlir

template <class OpTy> struct CreateHelper {
  mlir::OpBuilder builder;
  mlir::Location loc;

private:
  CreateHelper() = delete;
  CreateHelper(CreateHelper &&) = delete;
  CreateHelper(CreateHelper const &) = delete;

public:
  CreateHelper(mlir::OpBuilder builder, mlir::Location loc)
      : builder(builder), loc(loc) {}
};

// clang-format off
#define CREATE_OP(__type, Builder, Loc) [&]() {                                \
    using __TYPE = __type;                                                     \
    CreateHelper<__type> __builder{Builder, Loc};                              \
    OperationState __s{__builder.loc, __type::getOperationName()};             \
    __type::build(__builder.builder, __s,

#define END_OP );                                                              \
  return dyn_cast<__TYPE>(__builder.builder.create(__s));                      \
  }                                                                            \
  ()

// Help with op autocompletion. Takes op type, builder, location and then
// YourOp::build() arguments. Works fine with clangd.
#define CREATE(__type, __builder, __loc, ... ) CREATE_OP(__type, __builder, __loc) __VA_ARGS__ END_OP

// clang-format on
