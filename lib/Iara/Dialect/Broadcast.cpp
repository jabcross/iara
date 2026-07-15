#include "Iara/Dialect/IaraOps.h"
#include "Iara/Dialect/Join.h"
#include "Iara/Passes/Canonicalize/IaraCanonicalizePass.h"
#include "Iara/Util/CompilerTypes.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/OpCreateHelper.h"
#include "Iara/Util/Range.h"
#include <cassert>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/Interfaces/DataLayoutInterfaces.h>

namespace iara::dialect::broadcast {

using namespace iara::util::mlir;
using namespace iara::util::range;

std::string getBroadcastName(Type input_type,
                             llvm::SmallVector<Type> output_types,
                             DataLayout data_layout,
                             bool force_copy) {
  std::string name =
      llvm::formatv("iara_broadcast_{0}", stringifyType(input_type));

  for (auto [i, type] : llvm::enumerate(output_types)) {
    auto input_type_size = getTypeSize(input_type, data_layout);
    auto type_size = getTypeSize(type, data_layout);
    assert(type_size > 0);
    assert(type_size % input_type_size == 0);
    if (i == 0 && type_size == input_type_size && !force_copy) {
      name += "_1io";
      continue;
    }
    name += llvm::formatv("_{0}", type_size / input_type_size);
  }

  return name;
}

LLVM::LLVMFuncOp getOrCodegenBroadcastImpl(NodeOp broadcast) {

  assert(broadcast.getAllInputs().size() == 1);

  auto input = broadcast.getAllInputs().front();

  assert(llvm::isa<RankedTensorType>(input.getType()));

  auto outputs = broadcast.getAllOutputs();

  for (auto output : outputs) {
    assert(getTypeSize(output) % getTypeSize(input) == 0);
  }

  auto module = broadcast->getParentOfType<ModuleOp>();

  // broken?
  // if (auto existing = module.lookupSymbol(name)) {
  //   return dyn_cast<func::FuncOp>(existing);
  // }

  for (auto &op : module.getOps()) {
    auto f = llvm::dyn_cast<LLVM::LLVMFuncOp>(&op);
    if (!f)
      continue;
    if (f.getSymName() == broadcast.getImpl()) {
      return f;
    }
  }

  auto module_builder = OpBuilder(module);

  module_builder.setInsertionPointToStart(module.getBody());

  SmallVector<Type> arg_types;

  auto opaque_ptr = LLVM::LLVMPointerType::get(module_builder.getContext());

  auto num_args = 1 + outputs.size();

  if (getTypeSize(outputs.front()) == getTypeSize(input) &&
      !broadcast.getInout().empty()) {
    num_args--;
  }

  for (auto i = 0; i < num_args; i++) {
    arg_types.push_back(opaque_ptr);
  }

  auto impl = CREATE(
      LLVM::LLVMFuncOp,
      module_builder,
      broadcast.getLoc(),
      broadcast.getImpl(),
      LLVM::LLVMFunctionType::get(
          LLVM::LLVMVoidType::get(module_builder.getContext()), arg_types));
  impl.setVisibility(mlir::SymbolTable::Visibility::Public);
  impl->setAttr("llvm.emit_c_interface", module_builder.getUnitAttr());

  auto body = impl.addEntryBlock(module_builder);
  auto impl_builder = OpBuilder(impl);
  impl_builder.setInsertionPointToStart(body);

  auto size_val = util::mlir::getIntConstant(
      &impl.getFunctionBody().front(),
      impl_builder.getI64IntegerAttr(getTypeSize(input)));

  auto src = body->getArgument(0);

  // Determine whether output[0] is inout (reuses input buffer, no copy arg
  // generated for it). If so, copy outputs start at outputs[1].
  bool has_inout = !broadcast.getInout().empty() &&
                   getTypeSize(outputs.front()) == getTypeSize(input);
  size_t output_start = has_inout ? 1 : 0;

  for (auto [i, argument] :
       llvm::enumerate(body->getArguments().drop_front(1))) {

    // Use the broadcast output Value's type size, not the opaque-pointer
    // argument size. getTypeSize(!llvm.ptr) == 8, which causes integer
    // division to zero for any output type larger than 8 bytes.
    size_t num_copies =
        getTypeSize(outputs[output_start + i]) / getTypeSize(input);

    for (auto offset = 0; offset < num_copies; offset++) {
      Value dst = argument;
      if (offset > 0) {
        auto byte_offset = offset * getTypeSize(input);
        auto offset_val = impl_builder.create<LLVM::ConstantOp>(
            impl->getLoc(), impl_builder.getI64Type(),
            impl_builder.getI64IntegerAttr(byte_offset));
        dst = CREATE(LLVM::GEPOp,
                     impl_builder,
                     impl->getLoc(),
                     opaque_ptr,
                     impl_builder.getI8Type(),
                     dst,
                     ValueRange{offset_val});
      }
      CREATE(LLVM::MemcpyOp,
             impl_builder,
             impl->getLoc(),
             dst,
             src,
             size_val,
             true);
    }
  }

  CREATE(LLVM::ReturnOp, impl_builder, impl->getLoc(), ValueRange{});

  return impl;
}

NodeOp specializeBroadcast(NodeOp generic_broadcast, bool force_copy) {
  assert(generic_broadcast.getAllInputs().size() == 1);
  auto name =
      getBroadcastName(generic_broadcast.getAllInputs().front().getType(),
                       generic_broadcast->getResultTypes() | IntoVector(),
                       DataLayout::closest(generic_broadcast),
                       force_copy);
  generic_broadcast.setImpl(name);
  return generic_broadcast;
}

bool broadcastOutputsAllReadOnly(NodeOp broadcast) {
  // Every fanned-out output must be borrowed read-only: its consumer takes the
  // value in its `in` (read-only) segment, not `inout` (read-write). Follows
  // one EdgeOp hop (SIFT topologies wire broadcast->edge->consumer; auto-
  // inserted broadcasts do too by the time this runs). Any non-node user, a
  // dangling output, or an inout consumer disqualifies the broadcast.
  for (auto out : broadcast.getAllOutputs()) {
    for (auto *user : out.getUsers()) {
      NodeOp consumer;
      Value consumed = out;
      if (auto edge = llvm::dyn_cast<EdgeOp>(user)) {
        auto users = edge.getOut().getUsers();
        if (users.empty())
          return false;
        consumer = llvm::dyn_cast<NodeOp>(*users.begin());
        consumed = edge.getOut();
      } else {
        consumer = llvm::dyn_cast<NodeOp>(user);
      }
      if (!consumer)
        return false;
      if (!llvm::is_contained(consumer.getIn(), consumed))
        return false;
    }
  }
  return true;
}

// True when every use of `value` is a read-only consumer: the using operand
// lives in the owner node's `in` (read-only) segment, not `inout`. Classifies
// before the broadcast/edges are formed (operands still point at `value`).
bool usesAllReadOnly(Value value) {
  for (auto &use : value.getUses()) {
    auto consumer = llvm::dyn_cast<NodeOp>(use.getOwner());
    if (!consumer)
      return false;
    if (!llvm::is_contained(consumer.getIn(), value))
      return false;
  }
  return true;
}

// Rebuild `consumer` with one extra `none` (logic) output. Returns the new
// logic result Value. Used to signal a join once a read-only reader finishes.
static Value addLogicOutput(NodeOp consumer) {
  OpBuilder builder(consumer);
  SmallVector<Type> result_types(consumer.getResultTypes().begin(),
                                 consumer.getResultTypes().end());
  result_types.push_back(NoneType::get(builder.getContext()));
  auto nw = CREATE(NodeOp,
                   builder,
                   consumer.getLoc(),
                   result_types,
                   consumer.getImpl(),
                   consumer.getParams(),
                   /*in=*/consumer.getIn(),
                   /*inout=*/consumer.getInout());
  nw->setDiscardableAttrs(consumer->getDiscardableAttrDictionary());
  for (auto [old, _new] : llvm::zip(consumer.getResults(), nw.getResults()))
    old.replaceAllUsesWith(_new);
  consumer->erase();
  return nw.getResults().back();
}

// All-read-only zero-copy fan-out. `value` has N read-only uses. Build:
//   value --inout--> broadcast --passthrough(RW)--> join      (owns the buffer)
//                              --borrow_i---------> consumer_i (aliases, no copy)
//   consumer_i --logic--> join
// The broadcast is a special runtime node (fireBroadcast) that aliases its one
// input to every reader; the join owns the buffer and, gated by all N logic
// tokens (W5), frees it once after every reader finishes (GMMN generates the
// dealloc). Borrow output edges are tagged `borrow` so GMMN/codegen treat them
// as read-only aliases, not chain buffers. Returns the broadcast node.
NodeOp insertBroadcastBorrow(Value value) {
  OpBuilder builder(value.getDefiningOp());
  builder.setInsertionPointAfter(value.getDefiningOp());

  // Ordered uses (program order), mirroring insertBroadcast.
  auto uses = value.getUses() | Pointers() | IntoVector();
  Vec<OpOperand *> ordered_uses;
  for (auto &op : value.getDefiningOp()->getParentRegion()->getOps()) {
    for (auto use : uses) {
      if (use->getOwner() != &op)
        continue;
      ordered_uses.push_back(use);
      break;
    }
  }
  assert(ordered_uses.size() == uses.size());
  size_t N = ordered_uses.size();

  // ponytail: assumes each reader consumes `value` once (distinct owners), so
  // the rebuild below never invalidates a not-yet-processed OpOperand*. True for
  // every current topology; revisit if a node reads the same fan-out twice.

  auto ty = value.getType();
  SmallVector<Type> result_types(N + 1, ty); // [passthrough, borrow_0..borrow_{N-1}]
  auto bcast = CREATE(NodeOp,
                      builder,
                      value.getDefiningOp()->getLoc(),
                      result_types,
                      "iara_bcast_borrow",
                      /*params=*/ValueRange{},
                      /*in=*/ValueRange{},
                      /*inout=*/ValueRange{value});
  bcast->setAttr("broadcast_borrow", builder.getUnitAttr());

  Value passthrough = bcast.getResult(0);

  SmallVector<Value> logic_inputs;
  for (size_t i = 0; i < N; i++) {
    Value borrow = bcast.getResult(i + 1);
    ordered_uses[i]->set(borrow); // consumer now reads the borrow alias
    NodeOp consumer = cast<NodeOp>(ordered_uses[i]->getOwner());
    logic_inputs.push_back(addLogicOutput(consumer));
  }

  iara::dialect::insertJoin(passthrough, logic_inputs);

  // Expand implicit edges for every raw connection (mirrors insertBroadcast).
  if (!isa<EdgeOp>(value.getDefiningOp()))
    passes::canonicalize::expandImplicitEdge(value);
  for (auto res : bcast.getResults()) {
    auto users = res.getUsers() | IntoVector();
    assert(users.size() == 1);
    if (!isa<EdgeOp>(users.front()))
      passes::canonicalize::expandImplicitEdge(res);
  }
  for (auto logic : logic_inputs) {
    auto users = logic.getUsers() | IntoVector();
    assert(users.size() == 1);
    if (!isa<EdgeOp>(users.front()))
      passes::canonicalize::expandImplicitEdge(logic);
  }
  // Tag borrow output edges (results 1..N) as read-only aliases.
  for (size_t i = 0; i < N; i++) {
    auto edge = cast<EdgeOp>(*bcast.getResult(i + 1).getUsers().begin());
    edge->setAttr("borrow", builder.getUnitAttr());
  }
  return bcast;
}

NodeOp insertBroadcast(Value value, bool force_copy) {
  OpBuilder builder(value.getDefiningOp());
  builder.setInsertionPointAfter(value.getDefiningOp());

  Vec<Value> in, inout;

  if (force_copy)
    in.push_back(value);
  else
    inout.push_back(value);

  // SmallVector<Type> outputs;

  // getUses does not return users in topological order, for some reason.
  // While it technically doesn't matter, it's nice for debugging.
  auto uses = value.getUses() | Pointers() | IntoVector();
  Vec<OpOperand *> ordered_uses;

  for (auto &op : value.getDefiningOp()->getParentRegion()->getOps()) {
    for (auto use : uses) {
      if (use->getOwner() != &op)
        continue;
      ordered_uses.push_back(use);
      break;
    }
  }

  auto name = getBroadcastName(value.getType(),
                               SmallVector<Type>(uses.size(), value.getType()),
                               DataLayout::closest(value.getDefiningOp()),
                               force_copy);

  assert(ordered_uses.size() == uses.size());

  auto output_types = SmallVector<Type>(uses.size(), value.getType());

  auto broadcast_op = CREATE(NodeOp,
                             builder,
                             value.getDefiningOp()->getLoc(),
                             output_types,
                             name,
                             {},
                             in,
                             inout);

  for (auto [new_value, operand] :
       llvm::zip_equal(broadcast_op.getOut(), uses)) {

    // llvm::errs() << "Operand index: " << operand->getOperandNumber() << "\n";
    // llvm::errs() << "Result index: " << new_value.getResultNumber() << "\n";
    // operand->getOwner()->dump();
    operand->set(new_value);
  }

  assert((value.getUses() | Pointers() | Count()) == 1);

  if (!isa<EdgeOp>(value.getDefiningOp())) {
    passes::canonicalize::expandImplicitEdge(value);
  }
  for (auto output : broadcast_op.getAllOutputs()) {
    auto users = output.getUsers() | IntoVector();
    assert(users.size() == 1);
    if (!isa<EdgeOp>(users.front())) {
      passes::canonicalize::expandImplicitEdge(output);
    }
  }

  return broadcast_op;
}

} // namespace iara::dialect::broadcast
