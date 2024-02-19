include(AddMLIRPython)

# Specifies that all MLIR packages are co-located under the `mlir_iara`
# top level package (the API has been embedded in a relocatable way).
# TODO: Add an upstream cmake param for this vs having a global here.
add_compile_definitions("MLIR_PYTHON_PACKAGE_PREFIX=mlir_iara.")


################################################################################
# Sources
################################################################################

declare_mlir_python_sources(IaraPythonSources)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT IaraPythonSources
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir_iara"
  TD_FILE dialects/IaraOps.td
  SOURCES
    dialects/iara.py
  DIALECT_NAME iara)

declare_mlir_python_extension(IaraPythonSources.Extension
  MODULE_NAME _iaraDialects
  ADD_TO_PARENT IaraPythonSources
  SOURCES
    IaraExtension.cpp
  EMBED_CAPI_LINK_LIBS
    IaraCAPI
)

################################################################################
# Instantiation of all Python modules
################################################################################

add_mlir_python_modules(IaraPythonModules
  ROOT_PREFIX "${MLIR_BINARY_DIR}/python_packages/iara/mlir_iara"
  INSTALL_PREFIX "python_packages/iara/mlir_iara"
  DECLARED_SOURCES
    IaraPythonSources
    # TODO: Remove this in favor of showing fine grained registration once
    # available.
    MLIRPythonExtension.RegisterEverything
    MLIRPythonSources
  COMMON_CAPI_LINK_LIBS
    IaraPythonCAPI
  )
