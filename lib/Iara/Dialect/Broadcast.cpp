#include "Iara/Dialect/IaraOps.h"
#include "Iara/Dialect/Join.h"
#include "Iara/Dialect/Node.h"
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

// True when `value` fans out to read-only borrowers that each alias the WHOLE
// buffer 1:1. Every use must reach a consumer taking it in its `in` (read-only)
// segment. In canonicalized IARA every use goes through an EdgeOp, so follow
// that one hop to the consumer node.
//
// A multi-rate/gather edge (consumer reads more than the buffer holds) is the
// general zero-copy case too — each reader still gets a single alias, read
// through a toroidal (d)->(d mod L) memref map. That map must be materialized in
// the node wrapper (Chunk is already a memref descriptor for exactly this), and
// until that lands a multi-rate fan-out (edge out size != in size) stays on the
// copy path rather than reading a K*L view of an L-byte buffer out of bounds.
bool usesAllReadOnly(Value value) {
  for (auto &use : value.getUses()) {
    Operation *owner = use.getOwner();
    NodeOp consumer;
    Value consumed;
    if (auto edge = llvm::dyn_cast<EdgeOp>(owner)) {
      if (getTypeSize(edge.getOut()) != getTypeSize(edge.getIn()))
        return false; // multi-rate — needs the toroidal map (not yet wired)
      auto users = edge.getOut().getUsers();
      if (users.empty())
        return false;
      consumer = llvm::dyn_cast<NodeOp>(*users.begin());
      consumed = edge.getOut();
    } else {
      consumer = llvm::dyn_cast<NodeOp>(owner);
      consumed = value;
    }
    if (!consumer)
      return false;
    if (!llvm::is_contained(consumer.getIn(), consumed))
      return false;
  }
  return true;
}

static Value addLogicOutput(NodeOp consumer); // defined below

// A formed (explicit) broadcast is borrowable when every output is read-only
// (broadcastOutputsAllReadOnly) AND every output aliases the whole input 1:1
// (same byte size — a larger output is a multi-rate gather needing the toroidal
// map, not yet wired). SIFT's topology emits explicit `@iara_broadcast` nodes
// (not auto-inserted fan-outs), so this is the path that borrows its big buffers.
// Per-reader firing multiplicity for the join's logic edge: how many times the
// reader fires per broadcast firing (= per buffer instance). Derived from the
// repetition vector (node total_iter_firings), so it must run AFTER a static
// analysis pass. Returns 0 if the reader gathers across buffers (reader fires
// fewer than the broadcast) or re-reads the whole buffer (replication needing a
// toroidal map) — both unsupported by the plain per-buffer join, so callers skip
// such broadcasts.
static i64 readerMultiplicity(NodeOp broadcast, EdgeOp outEdge) {
  i64 bc_reps = Node(broadcast).totalIterFirings();
  auto consumer = llvm::cast<NodeOp>(*outEdge.getOut().getUsers().begin());
  i64 rd_reps = Node(consumer).totalIterFirings();
  // The reader must fire an integer number of times per buffer instance so its
  // multi-rate logic edge gates the join exactly once per buffer. A fractional
  // count means the reader gathers across buffers — unsupported. Replication
  // (reader re-reads the buffer, e.g. SIFT's L -> K*L broadcasts) IS supported:
  // fireBroadcast K-pushes the physical buffer with a toroidal (j*cons mod L)
  // offset, so no restriction on the read/buffer size ratio here.
  if (bc_reps <= 0 || rd_reps <= 0 || rd_reps % bc_reps != 0)
    return 0;
  return rd_reps / bc_reps;
}

bool broadcastIsPureBorrowable(NodeOp broadcast) {
  if (broadcast.getAllInputs().size() != 1)
    return false;
  // A delayed/feedback edge on the input means the buffer lives across graph
  // iterations (re-read with a delay); aliasing it is unsafe — leave it to the
  // copy path (breakLoops already handles feedback). Same for outputs.
  auto hasDelay = [](mlir::Value v) {
    for (auto *u : v.getUsers())
      if (auto e = llvm::dyn_cast<EdgeOp>(u))
        if (e->hasAttr("delay"))
          return true;
    if (auto e = llvm::dyn_cast_or_null<EdgeOp>(v.getDefiningOp()))
      if (e->hasAttr("delay"))
        return true;
    return false;
  };
  if (hasDelay(broadcast.getAllInputs().front()))
    return false;
  if (!broadcastOutputsAllReadOnly(broadcast))
    return false;
  // Every reader must have an integer firing multiplicity (see
  // readerMultiplicity). Replication (L -> K*L) and multi-firing readers are
  // supported: the borrow output keeps the original (logical) rate so the SDF is
  // unchanged from the copy graph, the join's multi-rate logic edges gate the
  // free, and fireBroadcast + the toroidal layout map alias the L-physical buffer
  // across the K logical reads.
  // The broadcast itself must fire exactly once per graph iteration. A
  // multi-firing broadcast (bc_reps > 1, e.g. SIFT's octave-loop counter/pyramid
  // feedback) reads a fresh input slice each firing; aliasing that with the
  // toroidal wrap (period L = one firing's input) and a global FIFO offset is not
  // yet correct (the input-slice offset is lost / the per-firing buffers alias
  // wrong). Single-firing broadcasts with multi-read readers (bc_reps==1, mult>1
  // -- the coefficient and big image replications, the actual RSS drivers) ARE
  // correct: one buffer, readers re-read it via the wrap. Restrict to those.
  if (Node(broadcast).totalIterFirings() != 1)
    return false;
  for (auto out : broadcast.getAllOutputs()) {
    if (hasDelay(out))
      return false; // feedback reader
    auto edge = llvm::cast<EdgeOp>(*out.getUsers().begin());
    if (readerMultiplicity(broadcast, edge) == 0)
      return false;
  }
  return true;
}

// Convert a formed all-read-only, same-size broadcast into the zero-copy borrow
// shape (see insertBroadcastBorrow for the auto-fanout equivalent). The broadcast
// already has output edges to its consumers, so redirect those edges to borrow
// aliases instead of re-expanding. Precondition: broadcastIsPureBorrowable.
NodeOp convertBroadcastToBorrow(NodeOp broadcast) {
  Value input = broadcast.getAllInputs().front();
  size_t N = broadcast.getResults().size();

  OpBuilder builder(broadcast);
  auto ty = input.getType();
  // result[0] = passthrough (owns the L-byte physical buffer, → join); the
  // borrows KEEP the original (logical) output types so the graph's rates are
  // identical to the copy broadcast (a replication output is K*L logical, aliased
  // to the L physical buffer at runtime via the toroidal layout map).
  SmallVector<Type> result_types;
  result_types.push_back(ty);
  for (size_t i = 0; i < N; i++)
    result_types.push_back(broadcast.getResult(i).getType());
  auto nb = CREATE(NodeOp,
                   builder,
                   broadcast.getLoc(),
                   result_types,
                   "iara_bcast_borrow",
                   /*params=*/ValueRange{},
                   /*in=*/ValueRange{},
                   /*inout=*/ValueRange{input});
  nb->setAttr("broadcast_borrow", builder.getUnitAttr());
  Value passthrough = nb.getResult(0);

  // Physical buffer period L (bytes): the reader's logical offset wraps mod L to
  // alias the one buffer. Carried as an affine layout map on each borrow edge and
  // collapsed to `offset mod L` at wrapper codegen (late), leaving room for
  // future map composition (e.g. transpose cancellation) before that.
  i64 buf_L = getTypeSize(input);
  auto d0 = mlir::getAffineDimExpr(0, builder.getContext());
  auto layout = mlir::AffineMap::get(1, 0, d0 % buf_L, builder.getContext());
  auto layoutAttr = mlir::AffineMapAttr::get(layout);

  // Redirect each output edge to read the borrow alias; give its consumer a
  // logic output feeding the join.
  SmallVector<Value> logic_inputs;
  SmallVector<i64> mults;
  for (size_t i = 0; i < N; i++) {
    auto edge = cast<EdgeOp>(*broadcast.getResult(i).getUsers().begin());
    edge->setOperand(0, nb.getResult(i + 1)); // edge now reads the borrow
    edge->setAttr("borrow", builder.getUnitAttr());
    edge->setAttr("layout", layoutAttr); // toroidal (d) -> (d mod L)
    // This reader fires `mult` times per buffer (from the repetition vector), so
    // its logic edge to the join is multi-rate: the join expects that many
    // increments before it fires (and frees). broadcastIsPureBorrowable already
    // verified every mult is a valid non-zero integer.
    i64 mult = readerMultiplicity(broadcast, edge);
    assert(mult > 0 && "convert called on a non-borrowable broadcast");
    mults.push_back(mult);
    auto consumer = cast<NodeOp>(*edge.getOut().getUsers().begin());
    logic_inputs.push_back(addLogicOutput(consumer));
  }

  auto join = iara::dialect::insertJoin(passthrough, logic_inputs);
  if (!isa<EdgeOp>(*passthrough.getUsers().begin()))
    passes::canonicalize::expandImplicitEdge(passthrough);
  // Materialize each reader->join logic edge as a MULTI-RATE edge: the reader
  // emits 1 token/firing (in tensor<1xi8>), the join consumes `mult` per firing
  // (out tensor<mult xi8>). The `logic_edge` tag marks it control-only (no
  // buffer, not a kernel arg); its 1:mult rate lives in the types, so
  // getFlowRatio gives join_firings = reader_firings/mult (see SDF.cpp).
  auto i8 = builder.getI8Type();
  for (size_t i = 0; i < N; i++) {
    Value logic_out = logic_inputs[i];
    OpBuilder eb(join);
    auto le = CREATE(EdgeOp,
                     eb,
                     join.getLoc(),
                     mlir::RankedTensorType::get({mults[i]}, i8),
                     logic_out);
    le->setAttr("logic_edge", builder.getUnitAttr());
    logic_out.replaceAllUsesExcept(le.getOut(), {le});
  }

  broadcast->erase();
  return nb;
}

// Rebuild `consumer` with one extra logic output: a `tensor<1xi8>` (one token
// per firing). The edge that will carry it is tagged `logic_edge`, which is what
// marks it control-only (the i8 type alone does not). Returns the new logic
// result Value. Used to signal a join once a read-only reader finishes.
static Value addLogicOutput(NodeOp consumer) {
  OpBuilder builder(consumer);
  SmallVector<Type> result_types(consumer.getResultTypes().begin(),
                                 consumer.getResultTypes().end());
  result_types.push_back(
      mlir::RankedTensorType::get({1}, builder.getI8Type()));
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

  // Redirect each reader to its borrow alias. The use may be an edge operand
  // (canonicalized IARA) or, pre-canonicalization, a node operand.
  for (size_t i = 0; i < N; i++)
    ordered_uses[i]->set(bcast.getResult(i + 1));

  // Materialize edges for the input and the borrow results. Skip the passthrough
  // for now — it has no user until the join below is built.
  if (!isa<EdgeOp>(value.getDefiningOp()))
    passes::canonicalize::expandImplicitEdge(value);
  for (size_t i = 0; i < N; i++) {
    Value borrow = bcast.getResult(i + 1);
    if (!isa<EdgeOp>(*borrow.getUsers().begin()))
      passes::canonicalize::expandImplicitEdge(borrow);
  }

  // Each borrow now flows broadcast -> edge -> consumer node. Tag the edge and
  // give the consumer a logic output feeding the join.
  SmallVector<Value> logic_inputs;
  for (size_t i = 0; i < N; i++) {
    auto borrow_edge =
        cast<EdgeOp>(*bcast.getResult(i + 1).getUsers().begin());
    borrow_edge->setAttr("borrow", builder.getUnitAttr());
    auto consumer = cast<NodeOp>(*borrow_edge.getOut().getUsers().begin());
    logic_inputs.push_back(addLogicOutput(consumer));
  }

  // Join owns the buffer (RW passthrough) and gates on all readers finishing.
  iara::dialect::insertJoin(passthrough, logic_inputs);

  // Materialize the passthrough and logic-output edges. Each reader here consumes
  // `value` once (mult=1), so the logic edges are 1:1 (tensor<1xi8> both sides);
  // tag them `logic_edge` to mark them control-only.
  if (!isa<EdgeOp>(*passthrough.getUsers().begin()))
    passes::canonicalize::expandImplicitEdge(passthrough);
  for (auto logic : logic_inputs) {
    EdgeOp le = dyn_cast<EdgeOp>(*logic.getUsers().begin());
    if (!le)
      le = passes::canonicalize::expandImplicitEdge(logic);
    le->setAttr("logic_edge", builder.getUnitAttr());
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
