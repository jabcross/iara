#ifndef IARA_CODEGEN_STATIC_BUILDER_H
#define IARA_CODEGEN_STATIC_BUILDER_H

#include "Iara/Codegen/GetMLIRType.h"
#include "Iara/Util/OpCreateHelper.h"
#include "IaraRuntime/SDF_OoO_Node.h"
#include "mlir/IR/MLIRContext.h"
#include <Iara/Util/Mlir.h>
#include <boost/pfr/core.hpp>
#include <boost/pfr/tuple_size.hpp>
#include <cstddef>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributeInterfaces.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/IR/Value.h>
#include <mlir/IR/ValueRange.h>
#include <mlir/Support/LLVM.h>
#include <source_location>
#include <span>
#include <type_traits>
#include <utility>

namespace iara::codegen {
using namespace mlir;

template <class T>
concept ConvertibleToAttr = requires(T t) {
  { asAttr(std::declval<MLIRContext *>(), t) };
};

template <class T>
concept Reflectable = requires(T t) {
  { boost::pfr::structure_to_tuple(t) };
  { std::enable_if_t<(boost::pfr::tuple_size_v<T> > 1)>() };
};

template <class R>
  requires Reflectable<R>
struct StaticBuilder {
  ModuleOp module;
  MLIRContext *ctx;
  OpBuilder mod_builder;
  R *root_ptr;
  Location loc;
  StringRef root_global_name;

  Vec<std::pair<std::span<char>, std::string>> owned_buffers;
  Vec<std::function<void()>> pointers_to_stitch;
  std::map<void *, std::string> known_global_ptrs;
  std::map<void *, std::pair<i64, std::string>> pointer_lists;

  LLVM::GlobalOp makeCompoundGlobal(StringRef name, Type type, i64 align,
                                    std::function<void(OpBuilder, Value &)> f) {
    auto global_op = CREATE(LLVM::GlobalOp, mod_builder, loc, type, false,
                            LLVM::Linkage::External, name, {}, align);
    global_op.getInitializer().emplaceBlock();
    auto global_init_builder =
        OpBuilder::atBlockBegin(global_op.getInitializerBlock());

    Value value = CREATE(LLVM::UndefOp, global_init_builder, loc, type);

    f(global_init_builder, value);

    CREATE(LLVM::ReturnOp, global_init_builder, loc, value);

    return global_op;
  }

  void stitch_pointers() {
    for (auto f : pointers_to_stitch)
      f();
  }

  static std::pair<LLVM::GlobalOp, StaticBuilder>
  build(ModuleOp module, R *_root, StringRef _root_global_name,
        std::map<void *, std::string> known_global_ptrs, Location loc) {

    StaticBuilder rv{.module = module,
                     .ctx = module.getContext(),
                     .mod_builder = OpBuilder::atBlockBegin(module.getBody()),
                     .root_ptr = _root,
                     .loc = loc,
                     .root_global_name = _root_global_name,
                     .owned_buffers = {},
                     .pointers_to_stitch = {},
                     .known_global_ptrs = known_global_ptrs};
    rv.owned_buffers.push_back({{reinterpret_cast<char *>(_root), sizeof(R)},
                                {_root_global_name.str()}});
    auto type = getMLIRType<R>(rv.ctx);
    auto global_op =
        rv.makeCompoundGlobal(_root_global_name, type, alignof(R),
                              [&](OpBuilder builder, Value &value) {
                                value = rv.asStatic(builder, loc, *_root);
                              });
    rv.stitch_pointers();
    return {global_op, std::move(rv)};
  }

  std::string generateUniqueGlobalName(
      StringRef prefix,
      std::source_location source_loc = std::source_location::current()) {
    int counter = 0;
    for (auto named_op : module.getOps<SymbolOpInterface>()) {
      if (!named_op.getName().starts_with(prefix))
        continue;
      counter++;
    }
    return std::string(llvm::formatv("{0}_{1}", prefix, counter));
  }

  template <class T>
    requires ConvertibleToAttr<T>
  Value asStatic(OpBuilder value_builder, Location loc, T const &t) {
    auto attr = asAttr(value_builder.getContext(), t);
    return CREATE(LLVM::ConstantOp, value_builder, loc, attr);
  }

  // Spans are treated as independent buffers, and get their own memory. We need
  // to keep track that the new memory is valid.
  template <class T>
  Value asStatic(OpBuilder value_builder, Location loc, Span<T> const &t) {
    Value ptr;
    if (t.size() == 0) {
      ptr = CREATE(LLVM::ZeroOp, value_builder, loc,
                   LLVM::LLVMPointerType::get(value_builder.getContext()));
    } else {

      std::string prefix =
          llvm::formatv("iara_static_span_{0}_{1}", typeid(T).name(), t.size());
      auto name = generateUniqueGlobalName(prefix);

      llvm::errs() << "Allocating global span: " << name << "\n";
      auto array_type = LLVM::LLVMArrayType::get(
          getMLIRType<T>(mod_builder.getContext()), t.size());
      auto span = std::span<T>{t.ptr, t.extents};
      auto global_op = makeCompoundGlobal(
          name, array_type, alignof(T), [&](OpBuilder builder, Value &value) {
            for (auto [i, v] : llvm::enumerate(span)) {
              auto element = asStatic(builder, loc, v);
              value =
                  CREATE(LLVM::InsertValueOp, builder, loc, value, element, i)
                      .getResult();
            }
          });
      std::span<char> char_span{
          const_cast<char *>(reinterpret_cast<const char *>(&*span.begin())),
          const_cast<char *>(reinterpret_cast<const char *>(&*span.end()))};
      owned_buffers.push_back({char_span, name});

      ptr = CREATE(LLVM::AddressOfOp, value_builder, loc,
                   LLVM::LLVMPointerType::get(ctx), StringRef{name});
    }

    Value size = asStatic(value_builder, loc, t.size());
    auto struct_type = getMLIRType<Span<T>>(ctx);
    Value _struct = CREATE(LLVM::UndefOp, value_builder, loc, struct_type);
    _struct =
        CREATE(LLVM::InsertValueOp, value_builder, loc, _struct, ptr, {(i64)0});
    _struct = CREATE(LLVM::InsertValueOp, value_builder, loc, _struct, size,
                     {(i64)1});

    return _struct;
  }

  template <class T>
  Value asStatic(OpBuilder builder, Location loc, std::vector<T> const &t) {
    return asStatic(builder, loc, Span<T>::from(t));
  }

  Value regenRuntimePointer(OpBuilder builder, void *compiletime_pointer) {
    auto [offset, name] = pointer_lists[compiletime_pointer];
    Value buffer_start_ptr = CREATE(LLVM::AddressOfOp, builder, loc,
                                    LLVM::LLVMPointerType::get(ctx), name);
    Value as_int = CREATE(LLVM::PtrToIntOp, builder, loc, builder.getI64Type(),
                          buffer_start_ptr);
    Value offset_val = asStatic(builder, loc, offset);
    Value add = CREATE(LLVM::AddOp, builder, loc, as_int, offset_val);
    Value runtime_ptr =
        CREATE(LLVM::IntToPtrOp, builder, loc, buffer_start_ptr.getType(), add);
    return runtime_ptr;
  }

  template <class T>
  Value asStatic(OpBuilder value_builder, Location loc, T *const &ptr) {
    if (ptr == nullptr) {
      return asStatic(value_builder, loc, nullptr);
    }

    if (auto entry = known_global_ptrs.find((void *)ptr);
        entry != known_global_ptrs.end()) {
      return CREATE(LLVM::AddressOfOp, value_builder, loc,
                    LLVM::LLVMPointerType::get(value_builder.getContext()),
                    StringRef{entry->second});
    }

    auto placeholder = CREATE(LLVM::ZeroOp, value_builder, loc,
                              LLVM::LLVMPointerType::get(ctx));

    pointers_to_stitch.push_back([=, this]() {
      if (ptr == nullptr)
        return;
      llvm::errs() << "Stitching pointer of type " << typeid(T *).name()
                   << "\n";
      for (auto [span, name] : owned_buffers) {

        const char *byte_ptr = (const char *)ptr;

        // must be self-referential
        if (byte_ptr < (&*span.begin()) || byte_ptr >= (&*span.end()))
          continue;

        i64 offset = byte_ptr - &*span.begin();

        pointer_lists[(void *)byte_ptr] = {offset, name};

        Value ptr =
            regenRuntimePointer(OpBuilder(placeholder), (void *)byte_ptr);

        placeholder->replaceAllUsesWith(ValueRange{ptr});
        placeholder->erase();
        return;
      }
      llvm_unreachable("This pointer doesn't point to the original struct or "
                       "to a buffer it generated.");
    });

    return placeholder;
  }

  template <size_t I> struct Index {
    const static size_t val = I;
    constexpr inline operator size_t() const { return I; }
  };

  template <class I, class T> void for_each();

  template <class I = Index<0>, class T, class... Args>
  void for_each(std::tuple<T, Args...> *, auto f, I i = I{}) {
    f(((T *)nullptr), i);
    if constexpr (sizeof...(Args) > 0) {
      for_each((std::tuple<Args...> *)nullptr, f, Index<I::val + 1>{});
    }
  }

  template <class StructType>
    requires Reflectable<StructType>
  Value asStatic(OpBuilder value_builder, Location loc, const StructType &t) {
    return asStaticStruct(value_builder, loc, t);
  }

  template <class StructType>
    requires Reflectable<StructType>
  Value asStaticStruct(OpBuilder value_builder, Location loc,
                       const StructType &t) {
    LLVM::LLVMStructType struct_type =
        cast<LLVM::LLVMStructType>(getMLIRType<StructType>(ctx));
    auto _struct =
        CREATE(LLVM::UndefOp, value_builder, loc, struct_type).getResult();

    using Tuple [[maybe_unused]] =
        decltype(boost::pfr::structure_to_tuple(std::declval<StructType>()));

    for_each((Tuple *)nullptr, [&](auto *null_ptr, auto index) {
      auto const &value = boost::pfr::get<index>(t);
      auto static_val = asStatic(value_builder, loc, value);
      auto types = struct_type.getBody();

      auto correct_type = types[index];

      static_val.getType().dump();
      llvm::errs() << static_val.getType() << "->";
      correct_type.dump();

      if (static_val.getType().getTypeID() != correct_type.getTypeID()) {
        llvm::errs() << "break here?\n";
        auto static_val = asStatic(value_builder, loc, value);
      }

      _struct = CREATE(LLVM::InsertValueOp, value_builder, loc, _struct,
                       static_val, {index})
                    .getResult();
    });
    return _struct;
  }

  Value asStatic(OpBuilder value_builder, Location loc,
                 const std::nullptr_t &) {
    return CREATE(LLVM::ZeroOp, value_builder, loc,
                  cast<Type>(
                      LLVM::LLVMPointerType::get(value_builder.getContext())))
        .getRes();
  }
};

} // namespace iara::codegen
#endif
