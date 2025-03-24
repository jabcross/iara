llvm_canonicalize_cmake_booleans(
        MLIR_ENABLE_BINDINGS_PYTHON
)

configure_lit_site_cfg(
        ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
        ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
        MAIN_CONFIG
        ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py
)

set(IARA_TEST_DEPENDS
        FileCheck count not
        iara-opt
        )
if(MLIR_ENABLE_BINDINGS_PYTHON)
  list(APPEND IARA_TEST_DEPENDS IaraPythonModules)
endif()

add_lit_testsuite(check-iara "Running the iara regression tests"
        ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS ${IARA_TEST_DEPENDS}
        )
set_target_properties(check-iara PROPERTIES FOLDER "test/Iara")

add_lit_testsuites(IARA ${CMAKE_CURRENT_SOURCE_DIR} DEPENDS ${IARA_TEST_DEPENDS})