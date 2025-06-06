#!/bin/sh

INPUT_FILENAME="$(basename $(realpath $1))"

INPUT_DIR=$(dirname $(realpath $1))

NO_EXT="${INPUT_FILENAME%.*}"

cd $INPUT_DIR

rm $NO_EXT.llvm.mlir

which mlir-opt

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
	$1 -o $NO_EXT.llvm.mlir

mlir-opt \
	--ensure-debug-info-scope-on-llvm-func \
	--mlir-print-debuginfo \
	$NO_EXT.llvm.mlir >$NO_EXT.llvm.mlir.2

which mlir-translate

mlir-translate --mlir-to-llvmir \
	$NO_EXT.llvm.mlir.2 -o $NO_EXT.ll

# # opt -S -passes=debugify $(realpath $INPUT_DIR/$NO_EXT.ll) -o $INPUT_DIR/$NO_EXT.debug.ll

# opt --strip-debug -S $INPUT_DIR/$NO_EXT.ll >$INPUT_DIR/$NO_EXT.strip.ll
