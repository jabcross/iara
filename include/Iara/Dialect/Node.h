#ifndef IARA_DIALECT_NODE_H
#define IARA_DIALECT_NODE_H

#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/CommonTypes.h"
#include "llvm/ADT/STLExtras.h"

namespace iara::dialect {

class Node;
class Edge;

// Lightweight value-type wrapper over EdgeOp. Implicitly converts to/from
// EdgeOp via public inheritance. Adds typed accessors backed by discardable
// attributes and topology helpers returning Node/Edge wrappers.
class Edge : public EdgeOp {
public:
  using EdgeOp::EdgeOp;
  Edge(EdgeOp op) : EdgeOp(op) {}

  // Topology — defined inline below (needs Node fully visible).
  Node producer();
  Node consumer();
  Edge nextInChain();    // chain successor or null Edge
  Edge prevInChain();    // chain predecessor or null Edge

  // Free-function pass-throughs as methods.
  i64 prodRateBytes();
  i64 consRateBytes();
  i64 delaySizeBytes();

  // Typed accessors over discardable attrs. Keys mirror
  // VirtualFIFO_Edge_StaticInfo field names. `id` is intentionally absent.
  i64 localIndex() const           { return getI64("local_index"); }
  void setLocalIndex(i64 v)        { setI64("local_index", v); }

  i64 prodRate() const             { return getI64("prod_rate"); }
  void setProdRate(i64 v)          { setI64("prod_rate", v); }

  i64 consRate() const             { return getI64("cons_rate"); }
  void setConsRate(i64 v)          { setI64("cons_rate", v); }

  i64 consArgIdx() const           { return getI64("cons_arg_idx"); }
  void setConsArgIdx(i64 v)        { setI64("cons_arg_idx", v); }

  i64 delayOffset() const          { return getI64("delay_offset"); }
  void setDelayOffset(i64 v)       { setI64("delay_offset", v); }

  i64 delaySize() const            { return getI64("delay_size"); }
  void setDelaySize(i64 v)         { setI64("delay_size", v); }

  i64 blockSizeWithDelays() const  { return getI64("block_size_with_delays"); }
  void setBlockSizeWithDelays(i64 v) { setI64("block_size_with_delays", v); }

  i64 blockSizeNoDelays() const    { return getI64("block_size_no_delays"); }
  void setBlockSizeNoDelays(i64 v) { setI64("block_size_no_delays", v); }

  i64 prodAlpha() const            { return getI64("prod_alpha"); }
  void setProdAlpha(i64 v)         { setI64("prod_alpha", v); }

  i64 prodBeta() const             { return getI64("prod_beta"); }
  void setProdBeta(i64 v)          { setI64("prod_beta", v); }

  i64 consAlpha() const            { return getI64("cons_alpha"); }
  void setConsAlpha(i64 v)         { setI64("cons_alpha", v); }

  i64 consBeta() const             { return getI64("cons_beta"); }
  void setConsBeta(i64 v)          { setI64("cons_beta", v); }

private:
  i64 getI64(mlir::StringRef k) const {
    return (*this)->getAttrOfType<mlir::IntegerAttr>(k).getInt();
  }
  void setI64(mlir::StringRef k, i64 v) {
    (*this)->setAttr(k, mlir::OpBuilder((*this)).getI64IntegerAttr(v));
  }
};

// Lightweight value-type wrapper over NodeOp. Implicitly converts to/from
// NodeOp via public inheritance. Adds topology helpers returning Edge/Node
// wrappers and typed accessors backed by discardable attributes.
class Node : public NodeOp {
public:
  using NodeOp::NodeOp;
  Node(NodeOp op) : NodeOp(op) {}

  // Topology — return ranges of Edge.
  auto pureInputs() {
    return llvm::map_range(getIn(), valueToEdge);
  }
  auto inoutInputs() {
    return llvm::map_range(getInout(), valueToEdge);
  }
  auto allInputs() {
    return llvm::map_range(NodeOp::getAllInputs(), valueToEdge);
  }
  auto allOutputs() {
    return llvm::map_range(NodeOp::getAllOutputs(), valueToOutEdge);
  }
  auto pureOutputs() {
    return llvm::map_range(getPureOuts(), valueToOutEdge);
  }

  // A logic (control-only) port is typed `none`: no data buffer, not a kernel
  // arg, only gates firing. Everything else is a data port.
  static bool isLogicValue(mlir::Value v) {
    return llvm::isa<mlir::NoneType>(v.getType());
  }
  auto dataIns() {
    return llvm::make_filter_range(
        getIn(), [](mlir::Value v) { return !isLogicValue(v); });
  }
  auto logicIns() {
    return llvm::make_filter_range(
        getIn(), [](mlir::Value v) { return isLogicValue(v); });
  }
  auto logicOuts() {
    return llvm::make_filter_range(
        getPureOuts(), [](mlir::Value v) { return isLogicValue(v); });
  }
  auto inoutOutputs() {
    // Result-side of inout = first getInout().size() results of NodeOp.
    return llvm::map_range(getResults().take_front(getInout().size()),
                           valueToOutEdge);
  }

  // (input-edge, output-edge) inout pairs.
  auto inoutPairs() {
    return llvm::map_range(getInoutPairs(*this), [](InoutPair p) {
      return std::pair<Edge, Edge>{toEdge(p.in), toOutEdge(p.out)};
    });
  }

  // Given the *output* side of an inout pair, returns the matching input Edge.
  // Null Edge if not an inout output.
  Edge matchingInoutInput(Edge outputEdge) {
    if (auto v = getMatchingInoutInput(outputEdge.getOut()))
      return toEdge(v);
    return {};
  }
  // Given the *input* side of an inout pair, returns the matching output Edge.
  // Null Edge if not an inout input.
  Edge matchingInoutOutput(Edge inputEdge) {
    if (auto v = getMatchingInoutOutput(inputEdge.getIn()))
      return toOutEdge(v);
    return {};
  }

  // Typed accessors over discardable attrs. Keys mirror
  // VirtualFIFO_Node_StaticInfo field names. `id` is intentionally absent.
  i64 argBytes() const             { return getI64("arg_bytes"); }
  void setArgBytes(i64 v)          { setI64("arg_bytes", v); }

  i64 numArgs() const              { return getI64("num_args"); }
  void setNumArgs(i64 v)           { setI64("num_args", v); }

  // Total logic-input tokens per firing (see RuntimeInfo::logic_in_bytes).
  // Defaults to 0 when unset: synthetic nodes (alloc/dealloc/broadcast) are
  // created outside annotateNodeInfo and have no logic inputs, so absent = 0.
  i64 logicInBytes() const {
    auto a = (*this)->getAttrOfType<mlir::IntegerAttr>("logic_in_bytes");
    return a ? a.getInt() : 0;
  }
  void setLogicInBytes(i64 v)      { setI64("logic_in_bytes", v); }

  i64 rank() const                 { return getI64("rank"); }
  void setRank(i64 v)              { setI64("rank", v); }

  i64 totalIterFirings() const     { return getI64("total_iter_firings"); }
  void setTotalIterFirings(i64 v)  { setI64("total_iter_firings", v); }

  i64 needsPriming() const         { return getI64("needs_priming"); }
  void setNeedsPriming(i64 v)      { setI64("needs_priming", v); }

private:
  // Input Value (operand of a NodeOp) is produced by an EdgeOp.
  static Edge toEdge(mlir::Value v) {
    return llvm::cast<EdgeOp>(v.getDefiningOp());
  }
  static Edge valueToEdge(mlir::Value v) { return toEdge(v); }

  // Output Value (result of a NodeOp) is consumed by exactly one EdgeOp.
  static Edge toOutEdge(mlir::Value v) {
    auto users = llvm::to_vector(v.getUsers());
    assert(users.size() == 1);
    return llvm::cast<EdgeOp>(users.front());
  }
  static Edge valueToOutEdge(mlir::Value v) { return toOutEdge(v); }

  i64 getI64(mlir::StringRef k) const {
    return (*this)->getAttrOfType<mlir::IntegerAttr>(k).getInt();
  }
  void setI64(mlir::StringRef k, i64 v) {
    (*this)->setAttr(k, mlir::OpBuilder((*this)).getI64IntegerAttr(v));
  }
};

// Out-of-line Edge methods that need Node fully defined.
inline Node Edge::producer() { return iara::getProducerNode(*this); }
inline Node Edge::consumer() { return iara::getConsumerNode(*this); }
inline Edge Edge::nextInChain() { return iara::followInoutChainForwards(*this); }
inline Edge Edge::prevInChain() { return iara::followInoutChainBackwards(*this); }
inline i64  Edge::prodRateBytes() { return iara::getProdRateBytes(*this); }
inline i64  Edge::consRateBytes() { return iara::getConsRateBytes(*this); }
inline i64  Edge::delaySizeBytes() { return iara::getDelaySizeBytes(*this); }

} // namespace iara::dialect

#endif
