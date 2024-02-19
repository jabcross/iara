add_mlir_dialect(IaraOps iara)
add_mlir_doc(IaraDialect IaraDialect Iara/ -gen-dialect-doc)
add_mlir_doc(IaraOps IaraOps Iara/ -gen-op-doc)

set(LLVM_TARGET_DEFINITIONS IaraPasses.td)
mlir_tablegen(IaraPasses.h.inc --gen-pass-decls)
add_public_tablegen_target(MLIRIaraPassesIncGen)
