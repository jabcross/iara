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

// A chunk of memory, laid out as MLIR's C-ABI rank-1 memref descriptor
// (MemRefDescriptor<i8,1> = { allocated, aligned, offset, sizes[1], strides[1] };
// see mlir/docs/TargetLLVMIR "C-compatible wrapper emission") followed by FIFO
// bookkeeping. Field names are kept from the pre-memref layout to limit churn:
//   allocated -> descriptor.allocated  (allocation base, free() target)
//   data      -> descriptor.aligned    (aligned/data pointer)
//   offset    -> descriptor.offset     (element offset; 0 for contiguous today)
//   data_size -> descriptor.sizes[0]
//   stride    -> descriptor.strides[0] (1 for contiguous)
// The layout is an affine map (identity today; a toroidal (d)->(d mod L) for
// multi-rate read-only edges). The map is compile-time and NOT an ABI field:
// per MLIR's memref calling convention it is resolved to index arithmetic
// (a `mod`/`remsi` + GEP) inside the node wrapper, which then hands the kernel a
// plain pointer. `virtual_offset` is the FIFO stream position and trails the
// descriptor (not part of the memref ABI). Scalar sizes[0]/strides[0] are
// byte-identical to the descriptor's i64[1] arrays.
struct VirtualFIFO_Chunk {

  i8 *allocated = nullptr;
  i8 *data = nullptr;
  i64 offset = 0;
  i64 data_size = 0;
  i64 stride = 1;
  i64 virtual_offset = 0;

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

  // Matches the struct field order above: the rank-1 memref descriptor
  // { allocated:ptr, aligned:ptr, offset:i64, sizes[0]:i64, strides[0]:i64 }
  // plus the trailing FIFO virtual_offset:i64. Scalar size/stride are
  // byte-identical to the descriptor's i64[1] arrays.
  inline static mlir::Type getMLIRType(mlir::MLIRContext *context) {
    auto type =
        mlir::LLVM::LLVMStructType::getIdentified(context, "VirtualFIFO_Chunk");
    if (!type.isInitialized()) {
      auto ptr = mlir::LLVM::LLVMPointerType::get(context);
      auto i64 = mlir::IntegerType::get(context, 64);
      auto res = type.setBody({ptr, ptr, i64, i64, i64, i64}, false);
      assert(res.succeeded());
    }
    return type;
  }

#endif
};

#endif
