#!/bin/sh

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
	--reconcile-unrealized-casts |
	mlir-translate --mlir-to-llvmir \
		--mlir-print-debuginfo
