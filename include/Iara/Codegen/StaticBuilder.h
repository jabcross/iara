#ifndef IARA_CODEGEN_STATIC_BUILDER_H
#define IARA_CODEGEN_STATIC_BUILDER_H

#include "Iara/Codegen/GetMLIRType.h"
#include "Iara/Util/OpCreateHelper.h"
#include "IaraRuntime/SDF_OoO_Node.h"
#include "mlir/IR/MLIRContext.h"
#include <Iara/Util/Mlir.h>
#include <boost/pfr/core.hpp>
#include <boost/pfr/core_name.hpp>
#include <boost/pfr/tuple_size.hpp>
#include <cstddef>
#include <llvm/ADT/Hashing.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributeInterfaces.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/IR/Value.h>
#include <mlir/IR/ValueRange.h>
#include <mlir/Interfaces/DataLayoutInterfaces.h>
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

  // std::vector<std::tuple<std::span<char>, std::string, Type>> owned_buffers;
  std::vector<std::function<void()>> pointers_to_stitch;
  std::reference_wrapper<std::map<void *, std::string>> known_global_ptrs;
  // std::map<void *, std::tuple<i64, std::string, Type>> pointer_lists;
  DenseMap<std::tuple<void *, Type, llvm::hash_code>,
           std::function<Value(OpBuilder, Location, void *)>>
      pointer_map;

  LLVM::GlobalOp makeCompoundGlobal(OpBuilder *dependency_builder,
                                    StringRef name, Type type, i64 align,
                                    std::function<void(OpBuilder, Value &)> f) {

    OpBuilder builder = [&]() {
      if (dependency_builder != nullptr) {
        auto top_level_op =
            dependency_builder->getInsertionBlock()->getParentOp();
        if (isa<ModuleOp>(top_level_op)) {
          return OpBuilder::atBlockBegin(module.getBody());
        }
        while (!isa<ModuleOp>(top_level_op->getParentOp()))
          top_level_op = top_level_op->getParentOp();

        return OpBuilder(top_level_op);
      }
      return OpBuilder::atBlockBegin(module.getBody());
    }();

    auto global_op = CREATE(LLVM::GlobalOp, builder, loc, type, false,
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
  build(ModuleOp module, R *_root, StringRef _root_global_name, Location loc,
        std::map<void *, std::string> &known_global_ptrs) {

    StaticBuilder rv{
        .module = module,
        .ctx = module.getContext(),
        .mod_builder = OpBuilder::atBlockBegin(module.getBody()),
        .root_ptr = _root,
        .loc = loc,
        .root_global_name = _root_global_name,
        // .owned_buffers = {},
        .pointers_to_stitch = {},
        .known_global_ptrs = known_global_ptrs,
        // .pointer_lists = {},
        .pointer_map = decltype(pointer_map){},
    };
    auto type = getMLIRType<R>(rv.ctx);
    /*     rv.owned_buffers.push_back({{reinterpret_cast<char *>(_root),
       sizeof(R)},
                                    {_root_global_name.str()},
                                    type}); */
    auto global_op = rv.makeCompoundGlobal(
        nullptr, _root_global_name, type, alignof(R),
        [&](OpBuilder builder, Value &value) {
          value =
              rv.asStatic(builder, loc, *_root, std::string{_root_global_name});
        });
    rv.stitch_pointers();
    return {global_op, std::move(rv)};
  }

  template <class T>
    requires ConvertibleToAttr<T>
  Value asStatic(OpBuilder value_builder, Location loc, T const &t,
                 std::string item_name = "") {
    auto attr = asAttr(value_builder.getContext(), t);
    return CREATE(LLVM::ConstantOp, value_builder, loc, attr);
  }

  // returns lambda that uses given opbuilder to generate a valid opaque
  // pointer.
  auto make_getter(std::string global_name, i64 offset, Type elem_type,
                   void *_ptr, bool allow_ptr = false) {
    if (!allow_ptr) {
      assert(elem_type.getTypeID() !=
             LLVM::LLVMPointerType::get(elem_type.getContext()).getTypeID());
    }
    auto getter = //
        [=](OpBuilder builder, Location loc, void *ptr) {
          if (ptr != _ptr) {
            llvm_unreachable("Different pointers mapping to same level");
          }
          auto ptr_to_elem_type = LLVM::LLVMPointerType::get(elem_type);
          auto opaque_ptr = LLVM::LLVMPointerType::get(builder.getContext());

          Value base =
              CREATE(LLVM::AddressOfOp, builder, loc, opaque_ptr, global_name);
          Value gep = CREATE(LLVM::GEPOp, builder, loc, opaque_ptr,
                             ptr_to_elem_type, base, {offset});
          return gep;
        };

    return getter;
  }

  // Special case for spans of pointers
  template <class T>
  Value asStatic(OpBuilder value_builder, Location loc, Span<T *> const &t,
                 std::string item_name) {

    auto elem_type = getMLIRType<T>(ctx);
    auto opaque_ptr_type = LLVM::LLVMPointerType::get(elem_type.getContext());
    assert(elem_type.getTypeID() != opaque_ptr_type.getTypeID());

    auto array_type = LLVM::LLVMArrayType::get(opaque_ptr_type, t.size());

    Value placeholder = CREATE(LLVM::ZeroOp, value_builder, loc,
                               LLVM::LLVMPointerType::get(ctx));
    placeholder.getDefiningOp()->setAttr("elem_type",
                                         TypeAttr::get(opaque_ptr_type));

    llvm::errs() << "Allocating global span: " << item_name << "\n";
    auto span = std::span<T *>{t.ptr, t.extents};

    if (t.size() > 0) {
      makeCompoundGlobal(
          &value_builder, item_name, array_type, alignof(T),
          [&](OpBuilder builder, Value &value) {
            for (auto [i, v] : llvm::enumerate(span)) {
              pointer_map[{(void *)&v, opaque_ptr_type,
                           llvm::hash_value(item_name)}] =
                  make_getter(item_name, i, elem_type, (void *)&v, true);
              auto element =
                  asStatic(builder, loc, v,
                           item_name + (std::string)llvm::formatv("__{0}", i));
              value =
                  CREATE(LLVM::InsertValueOp, builder, loc, value, element, i)
                      .getResult();
            }
          });
    }

    auto struct_type = getMLIRType<Span<T *>>(ctx);

    Value size = asStatic(value_builder, loc, t.size());
    Value _struct = CREATE(LLVM::UndefOp, value_builder, loc, struct_type);

    _struct = CREATE(LLVM::InsertValueOp, value_builder, loc, _struct,
                     placeholder, {(i64)0});
    _struct = CREATE(LLVM::InsertValueOp, value_builder, loc, _struct, size,
                     {(i64)1});

    if (t.size() > 0) {

      std::span<char> char_span{
          const_cast<char *>(reinterpret_cast<const char *>(&*span.begin())),
          const_cast<char *>(reinterpret_cast<const char *>(&*span.end()))};

      // owned_buffers.emplace_back(char_span, global_name, elem_type);
      pointers_to_stitch.push_back(
          [placeholder, t, item_name, opaque_ptr_type, loc, this]() {
            auto builder = OpBuilder(placeholder.getDefiningOp());
            Value first_item = pointer_map[{(void *)t.ptr, opaque_ptr_type,
                                            llvm::hash_value(item_name)}](
                builder, loc, (void *)t.ptr);
            placeholder.replaceAllUsesWith(first_item);
            placeholder.getDefiningOp()->erase();
            return;
          });
    }
    return _struct;
  }

  // Spans are treated as independent buffers, and get their own memory. We
  // need to keep track that the new memory is valid.
  template <class T>
  Value asStatic(OpBuilder value_builder, Location loc, Span<T> const &t,
                 std::string item_name) {

    auto elem_type = getMLIRType<T>(ctx);
    assert(elem_type.getTypeID() !=
           LLVM::LLVMPointerType::get(elem_type.getContext()).getTypeID());

    auto array_type = LLVM::LLVMArrayType::get(elem_type, t.size());

    Value placeholder = CREATE(LLVM::ZeroOp, value_builder, loc,
                               LLVM::LLVMPointerType::get(ctx));

    llvm::errs() << "Allocating global span: " << item_name << "\n";
    auto span = std::span<T>{t.ptr, t.extents};

    if (t.size() > 0) {
      makeCompoundGlobal(
          &value_builder, item_name, array_type, alignof(T),
          [&](OpBuilder builder, Value &value) {
            for (auto [i, v] : llvm::enumerate(span)) {
              pointer_map[{(void *)&v, elem_type,
                           llvm::hash_value(item_name)}] =
                  make_getter(item_name, i, elem_type, (void *)&v);
              auto element =
                  asStatic(builder, loc, v,
                           item_name + (std::string)llvm::formatv("__{0}", i));
              value =
                  CREATE(LLVM::InsertValueOp, builder, loc, value, element, i)
                      .getResult();
            }
          });
    }

    auto struct_type = getMLIRType<Span<T>>(ctx);

    Value size = asStatic(value_builder, loc, t.size());
    Value _struct = CREATE(LLVM::UndefOp, value_builder, loc, struct_type);

    _struct = CREATE(LLVM::InsertValueOp, value_builder, loc, _struct,
                     placeholder, {(i64)0});
    _struct = CREATE(LLVM::InsertValueOp, value_builder, loc, _struct, size,
                     {(i64)1});

    if (t.size() > 0) {

      std::span<char> char_span{
          const_cast<char *>(reinterpret_cast<const char *>(&*span.begin())),
          const_cast<char *>(reinterpret_cast<const char *>(&*span.end()))};

      // owned_buffers.emplace_back(char_span, global_name, elem_type);
      pointers_to_stitch.push_back(
          [placeholder, item_name, t, elem_type, loc, this]() {
            auto builder = OpBuilder(placeholder.getDefiningOp());
            Value first_item = pointer_map[{(void *)t.ptr, elem_type,
                                            llvm::hash_value(item_name)}](
                builder, loc, (void *)t.ptr);
            placeholder.replaceAllUsesWith(first_item);
            placeholder.getDefiningOp()->erase();
            return;
          });
    }
    return _struct;
  }

  template <class T>
  Value asStatic(OpBuilder builder, Location loc, std::vector<T> t,
                 std::string item_name) {
    return asStatic(builder, loc, Span<T>::from(t), item_name);
  }

  template <class T>
  Value asStatic(OpBuilder builder, Location loc, std::vector<T> *t,
                 std::string item_name) {
    return asStatic(builder, loc, Span<T>::from(*t), item_name);
  }

  template <class T>
  Value asStaticPtr(OpBuilder value_builder, Location loc, T *const &ptr,
                    std::string buffer_name = "") {
    if (ptr == nullptr) {
      return asStatic(value_builder, loc, nullptr);
    }

    Value placeholder = CREATE(LLVM::ZeroOp, value_builder, loc,
                               LLVM::LLVMPointerType::get(ctx));
    if (ptr != nullptr) {
      pointers_to_stitch.push_back([=, _this = this]() {
        {
          // capture for debug
          [[maybe_unused]] auto x = item_name;
        }
        if (auto entry = _this->known_global_ptrs.get().find((void *)ptr);
            entry != _this->known_global_ptrs.get().end()) {
          Value new_ptr = CREATE(
              LLVM::AddressOfOp, OpBuilder(placeholder.getDefiningOp()), loc,
              LLVM::LLVMPointerType::get(_this->ctx), entry->second);
          placeholder.replaceAllUsesWith(new_ptr);
          placeholder.getDefiningOp()->erase();
          return;
        }
        llvm::errs() << "Stitching pointer of type " << typeid(T *).name()
                     << "\n";
        Value new_ptr =
            _this->pointer_map[{(void *)ptr, getMLIRType<T>(_this->ctx),
                                llvm::hash_value(item_name)}](
                OpBuilder(placeholder.getDefiningOp()), loc, (void *)ptr);

        llvm::errs() << "Uses being replaced\n";
        for (auto &use : placeholder.getUses()) {
          use.getOwner()->dump();
        }

        placeholder.replaceAllUsesWith(new_ptr);
        placeholder.getDefiningOp()->erase();
        return;
      });
    }
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
  Value asStatic(OpBuilder value_builder, Location loc, const StructType &t,
                 std::string item_name) {
    LLVM::LLVMStructType struct_type =
        cast<LLVM::LLVMStructType>(getMLIRType<StructType>(ctx));
    auto _struct =
        CREATE(LLVM::UndefOp, value_builder, loc, struct_type).getResult();

    using Tuple [[maybe_unused]] =
        decltype(boost::pfr::structure_to_tuple(std::declval<StructType>()));

    for_each((Tuple *)nullptr, [&](auto *null_ptr, auto index) {
      auto const &value = boost::pfr::get<index>(t);
      std::string member_name{boost::pfr::get_name<index, StructType>()};
      std::string name{};
      auto static_val =
          asStatic(value_builder, loc, value, item_name + "__" + member_name);
      auto types = struct_type.getBody();

      auto correct_type = types[index];

      static_val.getType().dump();
      llvm::errs() << static_val.getType() << "->";
      correct_type.dump();

      // if (static_val.getType().getTypeID() != correct_type.getTypeID()) {
      //   llvm::errs() << "break here?\n";
      //   [[maybe_unused]] auto static_val = asStatic(value_builder, loc,
      //   value);
      // }

      _struct = CREATE(LLVM::InsertValueOp, value_builder, loc, _struct,
                       static_val, {index})
                    .getResult();
    });
    return _struct;
  }

  Value asStatic(OpBuilder value_builder, Location loc, const std::nullptr_t &,
                 std::string item_name = "") {
    return CREATE(LLVM::ZeroOp, value_builder, loc,
                  cast<Type>(
                      LLVM::LLVMPointerType::get(value_builder.getContext())))
        .getRes();
  }
};

} // namespace iara::codegen
#endif
