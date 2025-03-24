#!/bin/sh

INPUT_FILENAME="$(basename $1)"

INPUT_DIR=$(dirname "$1")

NO_EXT="${INPUT_FILENAME%.*}"

mlir-opt --convert-vector-to-scf \
	--convert-linalg-to-loops \
	--lower-affine \
	--convert-scf-to-cf \
	--canonicalize \
	--cse \
	--convert-vector-to-llvm \
	--convert-math-to-llvm \
	--expand-strided-metadata \
	--lower-affine \
	--finalize-memref-to-llvm \
	--convert-func-to-llvm="use-bare-ptr-memref-call-conv=1" \
	--convert-index-to-llvm \
	--convert-openmp-to-llvm \
	--reconcile-unrealized-casts \
	--mlir-print-debuginfo \
	--debugify-level=locations \
	$1 >$INPUT_DIR/$NO_EXT.llvm.mlir

# mlir-opt $INPUT_DIR/$NO_EXT.llvm.mlir >$INPUT_DIR/$NO_EXT.llvm.mlir

mlir-translate --mlir-to-llvmir \
	$INPUT_DIR/$NO_EXT.llvm.mlir >$INPUT_DIR/$NO_EXT.ll

# opt --strip-debug -S $INPUT_DIR/$NO_EXT.ll >$INPUT_DIR/$NO_EXT.strip.ll
