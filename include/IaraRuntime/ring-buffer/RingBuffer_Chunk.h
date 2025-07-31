#ifndef IARARUNTIME_RINGBUFFER_CHUNK_H
#define IARARUNTIME_RINGBUFFER_CHUNK_H

#include "Iara/Util/CommonTypes.h"
#include <cassert>
#include <cstdlib>

#ifdef IARA_COMPILER
  #include <mlir/Dialect/LLVMIR/LLVMDialect.h>
  #include <mlir/IR/MLIRContext.h>
  #include <mlir/IR/Types.h>
#endif

struct RingBuffer_Chunk {

  i8 *allocated = nullptr;
  i8 *data;
  i64 data_size;
};

#ifdef IARA_COMPILER

inline mlir::LLVM::LLVMStructType getChunkType(mlir::MLIRContext *context) {
  return mlir::LLVM::LLVMStructType::getLiteral(
      context,
      {
          mlir::LLVM::LLVMPointerType::get(context),
          mlir::LLVM::LLVMPointerType::get(context),
          mlir::IntegerType::get(context, 64),
      });
}

#endif

#endif
