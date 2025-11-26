# Experiment builds
# This is analogous to test/CMakeLists.txt but for experiment configurations

# Disable ccache for experiment builds (similar to test builds)
set(CMAKE_C_COMPILER_LAUNCHER "")
set(CMAKE_CXX_COMPILER_LAUNCHER "")

# Validate environment
if(NOT DEFINED ENV{LLVM_INSTALL})
    message(FATAL_ERROR "LLVM_INSTALL environment variable not set")
endif()

if(NOT DEFINED ENV{IARA_DIR} OR "$ENV{IARA_DIR}" STREQUAL "")
    set(ENV{IARA_DIR} "${PROJECT_SOURCE_DIR}")
endif()

# Find OpenMP (same as test/CMakeLists.txt)
set(OpenMP_CXX_FLAGS "-fopenmp")
find_library(OpenMP_CXX_LIBRARY
    NAMES omp libomp
    PATHS
        "$ENV{LLVM_INSTALL}/lib"
        "$ENV{LLVM_INSTALL}/lib64"
        "$ENV{LLVM_INSTALL}/lib/x86_64-unknown-linux-gnu"
    NO_DEFAULT_PATH
)

if(NOT OpenMP_CXX_LIBRARY)
    find_library(OpenMP_CXX_LIBRARY NAMES omp libomp)
endif()

if(NOT OpenMP_CXX_LIBRARY)
    message(FATAL_ERROR "Failed to locate libomp")
endif()

get_filename_component(OpenMP_CXX_LIBRARY_DIR "${OpenMP_CXX_LIBRARY}" DIRECTORY)

find_program(BASH_EXECUTABLE bash REQUIRED)

# Add cholesky experiment subdirectory
add_subdirectory(cholesky)
