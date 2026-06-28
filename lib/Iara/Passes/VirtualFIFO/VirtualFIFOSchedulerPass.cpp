#include "Iara/Dialect/IaraOps.h"
// #include "Iara/Passes/Common/Codegen/GetMLIRType.h"
#include "Iara/Passes/VirtualFIFO/BreakLoops.h"
#include "Iara/Passes/VirtualFIFO/Codegen/Codegen.h"
#include "Iara/Passes/VirtualFIFO/SDF/SDF.h"
#include "Iara/Passes/Common/Codegen/Codegen.h"
#include "Iara/Passes/VirtualFIFO/VirtualFIFOSchedulerPass.h"
#include "Iara/Util/EnvOption.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/OpCreateHelper.h"
#include "Iara/Util/Range.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <cstddef>
#include <system_error>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/LogicalResult.h>
#include <mlir/Conversion/ConvertToLLVM/ToLLVMPass.h>
#include <mlir/Conversion/FuncToLLVM/ConvertFuncToLLVMPass.h>
#include <mlir/Conversion/LLVMCommon/StructBuilder.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMAttrs.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/Dialect/LLVMIR/Transforms/Passes.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/DialectImplementation.h>
#include <mlir/IR/ExtensibleDialect.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/IR/Types.h>
#include <mlir/IR/Value.h>
#include <mlir/Pass/PassManager.h>
#include <mlir/Support/LogicalResult.h>

using namespace iara::util::range;
using namespace iara::passes::virtualfifo::sdf;
using namespace iara::util::mlir;

namespace iara::passes::virtualfifo {

using namespace func;
using namespace iara::passes::common::codegen;
using namespace iara::passes::virtualfifo::codegen;

struct VirtualFIFOSchedulerPass::Impl {
  VirtualFIFOSchedulerPass *pass;
  DenseMap<EdgeOp, Value> runtime_fifo_pointers;
  DenseMap<EdgeOp, LLVM::GlobalOp> edge_info_global_ops;
  DenseMap<NodeOp, LLVM::GlobalOp> node_infos;
  DenseMap<EdgeOp, LLVM::GlobalOp> delay_values;

  Impl(VirtualFIFOSchedulerPass *pass) : pass(pass) {}

  MLIRContext *ctx() { return &pass->getContext(); }

  Type i64type() { return IntegerType::get(ctx(), 64); }

  LLVM::LLVMPointerType llvm_pointer_type() {
    return LLVM::LLVMPointerType::get(ctx());
  }
  LLVM::LLVMVoidType llvm_void_type() { return LLVM::LLVMVoidType::get(ctx()); }

  LLVM::LLVMStructType chunk_type() {
    return cast<LLVM::LLVMStructType>(VirtualFIFO_Chunk::getMLIRType(ctx()));
  }


  LLVM::LLVMFunctionType wrapper_type() {
    return LLVM::LLVMFunctionType::get(
        llvm_void_type(),
        {IntegerType::get(ctx(), 64), iara::passes::common::codegen::getSpanType(ctx())},
        false);
  }

  // Node wrappers call the kernel function with prepopulated parameters, and
  // also spread out the arguments from the given array.
  // The two arguments are the sequence number and a pointer to a buffer of pointers corresponding to the actor ports.
  LLVM::LLVMFuncOp getOrCodegenNodeWrapper(ModuleOp module,
                                           OpBuilder mod_builder,
                                           NodeCodegenData node_codegen_data) {

    auto node_op = node_codegen_data.node_op;
    auto opaque_ptr_type = LLVM::LLVMPointerType::get(ctx());
    if (node_op.isAlloc()) {
      if (auto existing =
              module.lookupSymbol<LLVM::LLVMFuncOp>("iara_runtime_alloc")) {
        return existing;
      }
      auto rv = CREATE(LLVM::LLVMFuncOp,
                       mod_builder,
                       module.getLoc(),
                       "iara_runtime_alloc",
                       wrapper_type());
      rv.setVisibility(mlir::SymbolTable::Visibility::Private);
      rv->setAttr("llvm.emit_c_interface", mod_builder.getUnitAttr());
      return rv;
    }
    if (node_op.isDealloc()) {
      if (auto existing =
              module.lookupSymbol<LLVM::LLVMFuncOp>("iara_runtime_dealloc")) {
        return existing;
      }
      auto rv = CREATE(LLVM::LLVMFuncOp,
                       mod_builder,
                       module.getLoc(),
                       "iara_runtime_dealloc",
                       wrapper_type());
      rv.setVisibility(mlir::SymbolTable::Visibility::Private);
      rv->setAttr("llvm.emit_c_interface", mod_builder.getUnitAttr());
      return rv;
    }

    // Deduplicate by (impl, params): nodes sharing the same kernel and
    // compile-time params share a single wrapper + kernel_id.
    std::string sym_name = ("iara_node_wrapper_" + node_op.getImpl()).str();
    for (auto p : node_op.getParams()) {
      if (auto c = dyn_cast<arith::ConstantOp>(p.getDefiningOp())) {
        if (auto ia = dyn_cast<IntegerAttr>(c.getValue())) {
          sym_name += "_" + std::to_string(ia.getInt());
        }
      }
    }

    if (auto existing = module.lookupSymbol<LLVM::LLVMFuncOp>(sym_name)) {
      return existing;
    }

    auto num_buffers = node_op.getIn().size() + node_op.getOut().size();

    auto wrapper = CREATE(LLVM::LLVMFuncOp,
                          mod_builder,
                          node_op.getLoc(),
                          sym_name,
                          wrapper_type());

    wrapper.setVisibility(mlir::SymbolTable::Visibility::Public);
    wrapper->setAttr("llvm.emit_c_interface", mod_builder.getUnitAttr());

    // addEntryBlock broken?

    auto *block = &wrapper.getFunctionBody().emplaceBlock();
    auto _wrapper_type = wrapper_type();
    for (auto type : _wrapper_type.getParams()) {
      block->addArgument(type, wrapper->getLoc());
    }

    //   auto block = wrapper.addEntryBlock();

    auto func_builder = OpBuilder::atBlockBegin(block);

    // kernel must have format void name(params*, in*, inout*, out*).

    SmallVector<Value> kernel_args;

    for (auto p : node_op.getParams()) {
      auto constant = dyn_cast<arith::ConstantOp>(p.getDefiningOp());
      assert(constant && "Params should have been uniqued by now.");
      auto new_const = func_builder.clone(*constant);
      kernel_args.push_back(new_const->getResult(0));
    }

    for (size_t i = 0; i < num_buffers; i++) {

      // arg1 is a span struct. We want to get its data field.

      auto data_ptr = CREATE(LLVM::ExtractValueOp,
                                       func_builder,
                                       node_op.getLoc(),
                                       func_builder.getBlock()->getArgument(1), {0});

      // data_ptr is a pointer to the first chunk. we want the `data` field.

      // get pointer from array
      auto pointer_to_pointer = CREATE(LLVM::GEPOp,
                                       func_builder,
                                       node_op.getLoc(),
                                       opaque_ptr_type,
                                       chunk_type(),
                                       data_ptr,
                                       {(i32)i, 2 /* `data field` */});

      auto pointer_to_data = CREATE(LLVM::LoadOp,
                                    func_builder,
                                    node_op.getLoc(),
                                    opaque_ptr_type,
                                    pointer_to_pointer);

      kernel_args.push_back(pointer_to_data);
    }

    auto func = module.lookupSymbol(node_op.getImpl());

    if (!func) {
      auto kernel_call = CREATE(LLVM::CallOp,
                                func_builder,
                                node_op.getLoc(),
                                TypeRange{},
                                node_op.getImpl(),
                                kernel_args);
      ensureFuncDeclExists(kernel_call);
    } else if (auto llvm_func = dyn_cast<LLVM::LLVMFuncOp>(func)) {
      DEF_OP(auto,
             kernel_call,
             LLVM::CallOp,
             func_builder,
             node_op.getLoc(),
             TypeRange{},
             node_op.getImpl(),
             kernel_args);
    } else if (auto mlir_func = dyn_cast<func::FuncOp>(func)) {
      llvm_unreachable("No MLIR functions at this point,");
    }

    CREATE(LLVM::ReturnOp, func_builder, node_op.getLoc(), ValueRange{});

    return wrapper;
  }

  ActorOp getMainActor(ModuleOp module) {
    // At this point, there should be only one actor with a body.

    DenseSet<ActorOp> not_top_level;
    Vec<ActorOp> definitions;

    // CLI option > env var (IARA_MAIN_ACTOR) > auto-detect below.
    std::string main_actor_name = iara::util::optionOrEnv(
        pass->main_actor.hasValue(), pass->main_actor.getValue(),
        "IARA_MAIN_ACTOR");
    if (!main_actor_name.empty()) {

      auto actor = module.lookupSymbol<ActorOp>(main_actor_name);
      if (!actor) {
        llvm::errs() << "Provided actor name not found: " << main_actor_name
                     << "\n";
        llvm::errs() << "Available actors: \n";

        for (auto actor : module.getOps<ActorOp>()) {
          llvm::errs() << actor.getSymName() << "\n";
        }
      }

      assert(actor && "Provided actor name not found");
      return actor;
    }

    for (auto actor : module.getOps<ActorOp>()) {
      if (actor.isKernelDeclaration()) {
        continue;
      }
      if (not_top_level.contains(actor)) {
        continue;
      }
      definitions.push_back(actor);
    }
    if (definitions.size() > 1) {
      module->emitError("At this point, the graph should have had only one "
                        "top level candidate; there "
                        "are ")
          << definitions.size() << " here.";
      llvm_unreachable("");
    }
    if (definitions.size() == 0) {
      module->emitError("No actor definitions found.");
      llvm_unreachable("");
    }
    return definitions[0];
  }

  void codegenEdgeInit(OpBuilder builder, EdgeOp edge, VirtualFIFO_Edge &info) {
  }

  // Emit:
  //   void iara_dispatch_kernel(u8 kid, i64 seq, span<Chunk> args)
  // Uses llvm.switch: entry block switches on kid, each case block calls the
  // corresponding wrapper, default block is unreachable.
  void emitDispatchFn(ModuleOp module, OpBuilder mod_builder,
                      llvm::StringMap<u8> &kid_map) {
    auto loc = module.getLoc();
    auto u8_type  = IntegerType::get(ctx(), 8);
    auto span_type = iara::passes::common::codegen::getSpanType(ctx());

    auto fn_type = LLVM::LLVMFunctionType::get(
        llvm_void_type(), {u8_type, i64type(), span_type}, false);

    auto dispatch_fn = CREATE(LLVM::LLVMFuncOp, mod_builder, loc,
                              "iara_dispatch_kernel", fn_type);
    dispatch_fn.setVisibility(mlir::SymbolTable::Visibility::Public);
    dispatch_fn->setAttr("llvm.emit_c_interface", mod_builder.getUnitAttr());

    // Sort by kernel_id for deterministic output.
    SmallVector<std::pair<u8, std::string>> entries;
    for (auto &kv : kid_map)
      entries.push_back({kv.second, kv.first().str()});
    llvm::sort(entries, [](auto &a, auto &b) { return a.first < b.first; });

    // Build blocks.
    auto &body = dispatch_fn.getFunctionBody();
    auto *entry = &body.emplaceBlock();
    for (auto t : fn_type.getParams())
      entry->addArgument(t, loc);

    SmallVector<Block *> case_blocks;
    for (size_t i = 0; i < entries.size(); i++)
      case_blocks.push_back(&body.emplaceBlock());
    auto *default_block = &body.emplaceBlock();

    Value kid  = entry->getArgument(0);
    Value seq  = entry->getArgument(1);
    Value span = entry->getArgument(2);

    // Case values as APInt (u8).
    SmallVector<APInt> case_vals;
    for (auto &[kid_id, _] : entries)
      case_vals.push_back(APInt(8, kid_id));

    // Switch in entry block.
    {
      auto b = OpBuilder::atBlockBegin(entry);
      b.create<LLVM::SwitchOp>(loc, kid,
                                default_block, ValueRange{},
                                case_vals, BlockRange{case_blocks},
                                ArrayRef<ValueRange>(
                                    SmallVector<ValueRange>(case_blocks.size())));
    }

    // Each case block: call wrapper(seq, span) + ret.
    for (size_t i = 0; i < entries.size(); i++) {
      auto b = OpBuilder::atBlockBegin(case_blocks[i]);
      CREATE(LLVM::CallOp, b, loc, TypeRange{},
             entries[i].second, ValueRange{seq, span});
      CREATE(LLVM::ReturnOp, b, loc, ValueRange{});
    }

    // Default: unreachable.
    {
      auto b = OpBuilder::atBlockBegin(default_block);
      b.create<LLVM::UnreachableOp>(loc);
    }
  }

  void upgradeDelaysToDenseArrays(ActorOp actor) {
    for (auto edge : actor.getOps<EdgeOp>()) {
      if (!edge->hasAttr("delay")) {
        continue;
      }

      if (auto int_attr =
              llvm::dyn_cast_or_null<IntegerAttr>(edge["delay"].get())) {
        // fill with zeros
        auto elem_type = getElementTypeOrSelf(edge.getIn().getType());
        size_t num_elems = int_attr.getInt();
        size_t size_elem = getTypeSize(elem_type, DataLayout::closest(edge));
        char *zeros = (char *)calloc(num_elems, size_elem);
        auto delay_attr =
            DenseArrayAttr::get(elem_type,
                                int_attr.getInt(),
                                ArrayRef<char>(zeros, num_elems * size_elem));
        edge->setAttr("delay", delay_attr);
        free(zeros);
      }
    }
  }

  LogicalResult codegenStaticData(ActorOp actor, StaticAnalysisData &data) {

    // fill out infos

    auto module = actor->getParentOfType<ModuleOp>();
    auto mod_builder = OpBuilder::atBlockBegin(module.getBody());

    auto edge_ops = actor.getOps<EdgeOp>() | IntoVector();
    auto node_ops = actor.getOps<NodeOp>() | IntoVector();

    std::vector<NodeCodegenData> node_codegen_datas;
    std::vector<EdgeCodegenData> edge_codegen_datas;

    DenseMap<NodeOp, Vec<VirtualFIFO_Edge *>> input_fifos;
    DenseMap<NodeOp, Vec<VirtualFIFO_Edge *>> output_fifos;
    DenseMap<EdgeOp, ArrayRef<char>> delay_data;

    std::vector<char> dummy_ptrs(node_ops.size(), ' ');

    [[maybe_unused]] auto &x = llvm::errs();

    for (auto [i, node] : llvm::enumerate(node_ops)) {
      auto &codegen_data = node_codegen_datas.emplace_back();
      codegen_data.index = i;
      codegen_data.node_op = node;
    }

    for (auto [i, edge] : llvm::enumerate(edge_ops)) {
      auto &codegen_data = edge_codegen_datas.emplace_back();
      codegen_data.index = i;
      codegen_data.edge_op = edge;
    }

    // Create (or find) per-unique-kernel wrapper functions.
    // Assign kernel_id: maps deduped wrapper sym -> u8 id in emission order.
    llvm::StringMap<u8> kid_map;
    for (auto &node_pair : node_codegen_datas) {
      node_pair.wrapper = getOrCodegenNodeWrapper(module, mod_builder, node_pair);
      auto sym = node_pair.wrapper.getSymName().str();
      if (!kid_map.count(sym)) {
        assert(kid_map.size() < 256 && "kernel_id overflow: > 255 unique wrappers");
        kid_map[sym] = static_cast<u8>(kid_map.size());
      }
      node_pair.kernel_id = kid_map[sym];
    }

    // Emit the single dispatch function: switch on kernel_id → call wrapper.
    emitDispatchFn(module, mod_builder, kid_map);

    auto codegen_builder = CodegenStaticData(
        module, mod_builder, {node_codegen_datas}, {edge_codegen_datas});

    codegen_builder.codegenStaticData();

    auto actors = module.getOps<ActorOp>() | IntoVector();

    for (auto actor : actors) {
      actor->erase();
    }

    return success();
  }

  LogicalResult runOnOperation(ModuleOp module) {
    auto main_actor = getMainActor(module);

    upgradeDelaysToDenseArrays(main_actor);

    breakLoops(main_actor);

    auto static_analysis = sdf::analyzeAndAnnotate(main_actor);
    bool ok = llvm::succeeded(static_analysis);

    ok = ok && generateAllocsAndFrees(main_actor, *static_analysis).succeeded();

    // main_actor->dump();

    ok = ok && codegenStaticData(main_actor, *static_analysis).succeeded();
    emitRuntimeConfig();
    return success(ok);
  }

  void emitRuntimeConfig() {
    std::string sem = iara::util::optionOrEnv(
        pass->semaphore.hasValue(), pass->semaphore.getValue(),
        "IARA_SEMAPHORE", "sharded-hash");
    std::string define;
    if (sem == "atomic-ring")
      define = "#define IARA_SEMAPHORE_ATOMIC_RING 1\n";
    else if (sem == "global-mutex")
      define = "#define IARA_SEMAPHORE_GLOBAL_MUTEX 1\n";
    else if (sem == "sharded-hash")
      define = "#define IARA_SEMAPHORE_SHARDED_HASH 1\n";
    else {
      llvm::errs() << "Unknown --semaphore value '" << sem
                   << "', using sharded-hash\n";
      define = "#define IARA_SEMAPHORE_SHARDED_HASH 1\n";
    }
    std::error_code ec;
    llvm::raw_fd_ostream os("iara_runtime_config.h", ec);
    if (ec) {
      llvm::errs() << "Failed to write iara_runtime_config.h: " << ec.message() << "\n";
      return;
    }
    os << "// Generated by iara-opt VirtualFIFOSchedulerPass. Do not edit.\n"
       << "#ifndef IARA_RUNTIME_CONFIG_H\n#define IARA_RUNTIME_CONFIG_H\n"
       << "// semaphore = " << sem << "\n"
       << define
       << "#endif // IARA_RUNTIME_CONFIG_H\n";
  }
};

void VirtualFIFOSchedulerPass::runOnOperation() {
  pimpl = new Impl{this};
  if (pimpl->runOnOperation(getOperation()).failed()) {
    signalPassFailure();
  };
}

} // namespace iara::passes::virtualfifo
