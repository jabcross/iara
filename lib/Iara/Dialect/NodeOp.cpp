#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/Range.h"
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/LogicalResult.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/OpImplementation.h>
#include <mlir/IR/OperationSupport.h>
#include <mlir/IR/Value.h>
#include <mlir/Support/LLVM.h>
#include <optional>

namespace iara {

using namespace util::range;
using namespace util::mlir;

bool NodeOp::isInoutInput(OpOperand &operand) {
  assert(operand.getOwner() == getOperation());
  for (auto opd : this->getInout()) {
    if (opd == operand.get())
      return true;
  }
  return false;
}

bool NodeOp::isInoutOutput(OpResult &result) {
  assert(result.getOwner() == getOperation());
  if (result.getResultNumber() < getInout().size()) {
    return false;
  }
  return true;
}

bool NodeOp::isPureInInput(OpOperand &operand) {
  for (auto opd : this->getIn()) {
    if (opd == operand.get())
      return true;
  }
  return false;
}

llvm::SmallVector<Value> NodeOp::getAllInputs() {
  SmallVector<Value> rv;
  for (auto i : getIn()) {
    rv.push_back(i);
  }
  for (auto i : getInout()) {
    rv.push_back(i);
  }
  return rv;
}

llvm::SmallVector<mlir::Value> NodeOp::getAllOutputs() {
  return getOut() | Into<llvm::SmallVector<Value>>();
}

// Returns the operand that corresponds to this output, if they form an inout
// pair. Null otherwise.
Value NodeOp::getMatchingInoutInput(Value output) {
  for (auto [in, out] : getInoutPairs(*this)) {
    if (out == output)
      return in;
  }
  return nullptr;
}

// Returns the result that corresponds to this input, if they form an inout
// pair. Null otherwise.
Value NodeOp::getMatchingInoutOutput(Value input) {
  for (auto [in, out] : getInoutPairs(*this)) {
    if (in == input)
      return out;
  }
  return nullptr;
}

func::CallOp NodeOp::convertToCallOp() {
  auto call_op = CREATE(
      func::CallOp, OpBuilder{*this}, getLoc(), getImpl(), {}, getOperands());
  erase();
  return call_op;
}

FunctionType NodeOp::getKernelFunctionType() {
  auto builder = OpBuilder(*this);
  SmallVector<Type> types;

  for (auto p : getParams()) {
    types.push_back(p.getType());
  }
  for (auto v : getIn()) {
    types.push_back(v.getType());
  }
  for (auto v : getInout()) {
    types.push_back(v.getType());
  }
  for (auto v : getPureOuts()) {
    types.push_back(v.getType());
  }

  auto memref_types =
      llvm::to_vector(types | Map([](Type type) {
                        auto tensor = cast<RankedTensorType>(type);
                        return cast<Type>(MemRefType::get(
                            tensor.getShape(), tensor.getElementType()));
                      }));

  return builder.getFunctionType(memref_types, {});
}

bool NodeOp::signatureMatches(ActorOp actor) {
  auto actor_inputs = actor.getAllInputTypes();
  SmallVector<Value> node_inputs{getIn()};
  for (auto i : getInout())
    node_inputs.push_back(i);
  if (actor_inputs.size() != node_inputs.size()) {
    return false;
  }
  for (auto [a, b] : llvm::zip(actor_inputs, node_inputs)) {
    if (a != b.getType())
      return false;
  }

  auto actor_outputs = actor.getAllOutputTypes();
  SmallVector<Value> node_outputs{getOut()};

  if (actor_outputs.size() != node_outputs.size())
    return false;

  for (auto [a, b] : llvm::zip(actor_outputs, node_outputs)) {
    if (a != b.getType()) {
      return false;
    }
  }

  return true;
}

bool NodeOp::isAlloc() { return getImpl() == "iara_runtime_alloc"; }

bool NodeOp::isDealloc() { return getImpl() == "iara_runtime_dealloc"; }

::mlir::LogicalResult
NodeOp::verifySymbolUses(::mlir::SymbolTableCollection &symbolTable) {
  // Todo: better verification
  return success();
}

// Input rate in bytes
i64 getProdRateBytes(EdgeOp edge) { return getTypeSize(edge.getIn()); }

// Output rate in bytes
i64 getConsRateBytes(EdgeOp edge) { return getTypeSize(edge.getOut()); }

util::Rational getFlowRatio(EdgeOp edge) {
  return util::Rational(getTypeSize(edge.getIn()), getTypeSize(edge.getOut()));
}

NodeOp getProducerNode(EdgeOp edge) {
  return llvm::dyn_cast_if_present<NodeOp>(edge.getIn().getDefiningOp());
}
NodeOp getConsumerNode(EdgeOp edge) {
  auto users = llvm::to_vector(edge.getOut().getUsers());
  if (users.size() != 1)
    llvm_unreachable("Should have a consumer");
  return llvm::dyn_cast<NodeOp>(users[0]);
}

::mlir::ParseResult NodeOp::parse(::mlir::OpAsmParser &parser,
                                  ::mlir::OperationState &result) {

  ::mlir::FlatSymbolRefAttr implAttr;
  ::llvm::SmallVector<::mlir::OpAsmParser::UnresolvedOperand, 4> paramsOperands;
  ::llvm::SMLoc paramsOperandsLoc;
  (void)paramsOperandsLoc;
  ::llvm::SmallVector<::mlir::Type, 1> paramsTypes;
  ::llvm::SmallVector<::mlir::OpAsmParser::UnresolvedOperand, 4> inOperands;
  ::llvm::SMLoc inOperandsLoc;
  (void)inOperandsLoc;
  ::llvm::SmallVector<::mlir::Type, 1> inTypes;
  ::llvm::SmallVector<::mlir::OpAsmParser::UnresolvedOperand, 4> inoutOperands;
  ::llvm::SMLoc inoutOperandsLoc;
  (void)inoutOperandsLoc;
  ::llvm::SmallVector<::mlir::Type, 1> inoutTypes;
  ::llvm::SmallVector<::mlir::Type, 1> outTypes;

  auto parseTypedOperandList =
      [&](::llvm::SmallVector<::mlir::OpAsmParser::UnresolvedOperand, 4>
              &operands,
          ::llvm::SmallVector<::mlir::Type, 1> &types) -> ParseResult {
    bool using_paren = parser.parseOptionalLParen().succeeded();

    if (parser.parseCommaSeparatedList([&]() -> ParseResult {
          OpAsmParser::UnresolvedOperand unresolved;
          Type type;
          if (parser.parseOperand(unresolved) || parser.parseColonType(type))
            return failure();
          operands.push_back(unresolved);
          types.push_back(type);
          return success();
        }))
      return failure();

    if (using_paren && parser.parseRParen())
      return failure();
    return success();
  };

  auto parseTypeList =
      [&](::llvm::SmallVector<::mlir::Type, 1> &types) -> ParseResult {
    bool using_paren = parser.parseOptionalLParen().succeeded();

    if (parser.parseTypeList(types))
      return failure();

    if (using_paren && parser.parseRParen())
      return failure();

    return success();
  };

  if (parser.parseCustomAttributeWithFallback(
          implAttr, parser.getBuilder().getType<::mlir::NoneType>())) {
    return ::mlir::failure();
  }
  if (implAttr)
    result.getOrAddProperties<NodeOp::Properties>().impl = implAttr;
  if (::mlir::succeeded(parser.parseOptionalKeyword("params"))) {

    paramsOperandsLoc = parser.getCurrentLocation();

    if (parseTypedOperandList(paramsOperands, paramsTypes))
      return failure();
  }
  if (::mlir::succeeded(parser.parseOptionalKeyword("in"))) {

    inOperandsLoc = parser.getCurrentLocation();
    if (parseTypedOperandList(inOperands, inTypes))
      return ::mlir::failure();
  }
  if (::mlir::succeeded(parser.parseOptionalKeyword("inout"))) {

    inoutOperandsLoc = parser.getCurrentLocation();
    if (parseTypedOperandList(inoutOperands, inoutTypes))
      return ::mlir::failure();
  }
  if (::mlir::succeeded(parser.parseOptionalKeyword("out"))) {
    if (parseTypeList(outTypes))
      return ::mlir::failure();
  }
  {
    auto loc = parser.getCurrentLocation();
    (void)loc;
    if (parser.parseOptionalAttrDict(result.attributes))
      return ::mlir::failure();
    if (failed(verifyInherentAttrs(result.name, result.attributes, [&]() {
          return parser.emitError(loc)
                 << "'" << result.name.getStringRef() << "' op ";
        })))
      return ::mlir::failure();
  }
  ::llvm::copy(
      ::llvm::ArrayRef<int32_t>({static_cast<int32_t>(paramsOperands.size()),
                                 static_cast<int32_t>(inOperands.size()),
                                 static_cast<int32_t>(inoutOperands.size())}),
      result.getOrAddProperties<NodeOp::Properties>()
          .operandSegmentSizes.begin());
  result.addTypes(outTypes);
  if (parser.resolveOperands(
          paramsOperands, paramsTypes, paramsOperandsLoc, result.operands))
    return ::mlir::failure();
  if (parser.resolveOperands(
          inOperands, inTypes, inOperandsLoc, result.operands))
    return ::mlir::failure();
  if (parser.resolveOperands(
          inoutOperands, inoutTypes, inoutOperandsLoc, result.operands))
    return ::mlir::failure();
  return ::mlir::success();
}

namespace detail {
template <class T> void myprint(::mlir::OpAsmPrinter &_odsPrinter, T t) {
  _odsPrinter << t;
}
template <class A, class B>
void myprint(::mlir::OpAsmPrinter &_odsPrinter, std::tuple<A, B> p) {
  auto &[a, b] = p;
  _odsPrinter << a << " : " << b;
}
} // namespace detail

void NodeOp::print(::mlir::OpAsmPrinter &_odsPrinter) {

  auto printList = [&](StringRef name, auto items) {
    if (std::begin(items) != std::end(items)) {
      _odsPrinter.increaseIndent();
      _odsPrinter.printNewline();
      _odsPrinter << name;
      _odsPrinter.increaseIndent();
      for (auto i : items) {
        _odsPrinter.printNewline();
        iara::detail::myprint(_odsPrinter, i);
      }
      _odsPrinter.decreaseIndent();
      _odsPrinter.decreaseIndent();
    }
  };

  _odsPrinter << ' ';
  _odsPrinter.printAttributeWithoutType(getImplAttr());
  printList("params", zip(getParams(), getParams().getTypes()));
  printList("in", zip(getIn(), getIn().getTypes()));
  printList("inout", zip(getInout(), getInout().getTypes()));
  printList("out", getOut().getTypes());
  ::llvm::SmallVector<::llvm::StringRef, 2> elidedAttrs;
  elidedAttrs.push_back("operandSegmentSizes");
  elidedAttrs.push_back("impl");
  _odsPrinter.printOptionalAttrDict((*this)->getAttrs(), elidedAttrs);
}

} // namespace iara