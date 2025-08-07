#ifndef IARARUNTIME_VIRTUALFIFO_CHUNK_H
#define IARARUNTIME_VIRTUALFIFO_CHUNK_H

#include "Iara/Util/CommonTypes.h"
#include <cassert>
#include <cstdlib>

#ifdef IARA_COMPILER
  #include <mlir/Dialect/LLVMIR/LLVMDialect.h>
  #include <mlir/IR/MLIRContext.h>
  #include <mlir/IR/Types.h>
#endif

// todo: alignment

// #ifndef IARA_COMPILER

// extern std::unordered_map<void *, int> allocated_ptrs;

// #endif

// Type that represents a chunk of memory.
struct VirtualFIFO_Chunk {

  i8 *allocated = nullptr;
  i64 virtual_offset = 0;
  i8 *data;
  i64 data_size;

public:
  inline bool is_released() { return allocated == nullptr; }

  // Returns a portion of the beginning of the chunk, and shrinks this one
  // accordingly
  VirtualFIFO_Chunk take_front(i64 amount);

  // Returns a portion of the end of the chunk, and shrinks this one
  // accordingly
  VirtualFIFO_Chunk take_back(i64 amount);

  void release() { *this = VirtualFIFO_Chunk(); }

  static VirtualFIFO_Chunk make_empty() { return VirtualFIFO_Chunk(); }
  static VirtualFIFO_Chunk allocate(i64 size, i64 virtual_offset);

  inline bool is_empty() { return data_size == 0; }

#ifdef IARA_COMPILER

  inline static mlir::Type getMLIRType(mlir::MLIRContext *context) {
    auto type =
        mlir::LLVM::LLVMStructType::getIdentified(context, "VirtualFIFO_Chunk");
    if (!type.isInitialized()) {
      auto res = type.setBody(
          {
              mlir::LLVM::LLVMPointerType::get(context),
              mlir::IntegerType::get(context, 64),
              mlir::LLVM::LLVMPointerType::get(context),
              mlir::IntegerType::get(context, 64),
          },
          false);
      assert(res.succeeded());
    }
    return type;
  }

#endif
};

#endif
