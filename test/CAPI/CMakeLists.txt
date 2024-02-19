# Here we create a single aggregate shared library with the parts of the CAPI
# that we want to bundle together. Then we link a simple C executable
# against it to demonstrate that it does have the fully self contained
# core MLIR library and our own iara dialect.
add_mlir_aggregate(IaraCAPITestLib
  SHARED
  EMBED_LIBS
  MLIRCAPIIR
  # TODO: Remove this in favor of showing fine grained dialect registration
  # (once available).
  MLIRCAPIRegisterEverything
  IaraCAPI
)

add_llvm_executable(iara-capi-test
  iara-capi-test.c
)
llvm_update_compile_flags(iara-capi-test)
target_link_libraries(iara-capi-test
  PRIVATE IaraCAPITestLib)
