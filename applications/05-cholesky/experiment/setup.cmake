# Per-application CMake hook for Cholesky.
# Called by iara_add_test_instance() before compiling each instance.
# Use this for app-specific detection logic that the agnostic framework
# should not know about.

# OpenBLAS may be built with USE64BITINT (ILP64), making all LAPACK
# integer arguments 64-bit.  Detect this and add -DLAPACK_ILP64 so
# source code uses the correct lapack_int type.
#
# CMAKE_PREFIX_PATH comes from the environment via cmake -D switch,
# so it is colon-separated (POSIX style).  find_path needs a
# semicolon-separated CMake list, hence the REPLACE below.
string(REPLACE ":" ";" _prefix_list "${CMAKE_PREFIX_PATH}")
find_path(_openblas_include_dir NAMES openblas_config.h
    PATHS ${_prefix_list}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH)
if(_openblas_include_dir)
    file(READ "${_openblas_include_dir}/openblas_config.h" _openblas_config)
    if(_openblas_config MATCHES "OPENBLAS_USE64BITINT")
        # TEST_DEFINES is the cmake_parse_arguments prefix variable;
        # list(APPEND) works because include() runs in the function scope.
        list(APPEND TEST_DEFINES "LAPACK_ILP64")
    endif()
endif()
