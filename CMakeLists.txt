cmake_minimum_required(VERSION 3.20.0)

# Check that required environment variables are set
if(NOT DEFINED ENV{LLVM_INSTALL})
  message(FATAL_ERROR "LLVM_INSTALL environment variable not set. Please source .env first.")
endif()

if(NOT DEFINED ENV{LLVM_SOURCES})
  message(FATAL_ERROR "LLVM_SOURCES environment variable not set. Please source .env first.")
endif()

if(NOT DEFINED ENV{IARA_DIR})
  message(FATAL_ERROR "IARA_DIR environment variable not set. Please source .env first.")
endif()

if(NOT DEFINED ENV{PROJECTS_DIR})
  message(FATAL_ERROR "PROJECTS_DIR environment variable not set. Please source .env first.")
endif()

# Option to use Presburger-based buffer size calculation
option(IARA_USE_PRESBURGER_BUFFER_CALC
  "Use LLVM Presburger for buffer size calculation (experimental)"
  ON)

if(IARA_USE_PRESBURGER_BUFFER_CALC)
  add_compile_definitions(IARA_USE_PRESBURGER_BUFFER_CALC)
endif()

# Option to build tests via experiment framework (default: OFF)
# When ON, generates tests from experiments.yaml files and builds them
option(IARA_BUILD_TESTS
  "Generate and build tests from experiment framework (via generate_experiments.py)"
  OFF)

# Filters for test generation (only used when IARA_BUILD_TESTS=ON)
# IARA_EXPERIMENTS_FILTER: Filter applications by name pattern (e.g., "05-cholesky")
# IARA_EXPERIMENT_SETS_FILTER: Filter experiment sets by name (e.g., "regression_test")
set(IARA_EXPERIMENTS_FILTER "" CACHE STRING "Filter applications by name pattern")
set(IARA_EXPERIMENT_SETS_FILTER "" CACHE STRING "Filter experiment sets by name")

# Set compilers from environment BEFORE project() command
# This ensures CMake uses the correct compilers during compiler detection
set(CMAKE_C_COMPILER $ENV{LLVM_INSTALL}/bin/clang)
set(CMAKE_CXX_COMPILER $ENV{LLVM_INSTALL}/bin/clang++)

project(iara-dialect LANGUAGES CXX C)

set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR ON)

set(CMAKE_CXX_STANDARD 26 CACHE STRING "C++ standard to conform to")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_COMPILER_LAUNCHER ccache)
set(CMAKE_CXX_COMPILER_LAUNCHER ccache)

# Define simplified build types
# Available build types:
#   Debug          - Maximum debug information for development and debugging
#   Release        - Optimized for maximum performance (experiments, production)
#   RelWithDebInfo - Optimized with debug symbols for profiling

# Debug: Maximum debug info, no optimization
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -gdwarf" CACHE STRING "Debug CXX flags" FORCE)
set(CMAKE_C_FLAGS_DEBUG "-O0 -g -gdwarf" CACHE STRING "Debug C flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "" CACHE STRING "Debug linker flags" FORCE)

# Release: Maximum performance, no debug info
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native -fno-rtti -fno-exceptions -flto" CACHE STRING "Release CXX flags" FORCE)
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native -flto" CACHE STRING "Release C flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto" CACHE STRING "Release linker flags" FORCE)

# RelWithDebInfo: Optimized build with debug symbols for profiling
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -gdwarf -DNDEBUG -march=native -mtune=native -fno-exceptions -flto" CACHE STRING "RelWithDebInfo CXX flags" FORCE)
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -gdwarf -DNDEBUG -march=native -mtune=native -flto" CACHE STRING "RelWithDebInfo C flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "-flto" CACHE STRING "RelWithDebInfo linker flags" FORCE)

# Set build type from environment or default to Debug
if(NOT CMAKE_BUILD_TYPE)
  if(DEFINED ENV{CMAKE_BUILD_TYPE})
    set(CMAKE_BUILD_TYPE $ENV{CMAKE_BUILD_TYPE} CACHE STRING "Build type" FORCE)
  else()
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
  endif()
endif()

# Update the build type dropdown in cmake-gui
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
  "Debug" "Release" "RelWithDebInfo")


# Set LLVM/MLIR paths
if(NOT DEFINED MLIR_DIR)
  set(MLIR_DIR "$ENV{LLVM_SOURCES}/build/lib/cmake/mlir" CACHE PATH "Path to MLIR cmake config")
endif()

if(NOT DEFINED LLVM_DIR)
  set(LLVM_DIR "$ENV{LLVM_INSTALL}/lib/cmake/llvm" CACHE PATH "Path to LLVM cmake config")
endif()

# Set linker and other build options
# Using lld for better LTO support
set(LLVM_PARALLEL_LINK_JOBS 1 CACHE STRING "Number of parallel link jobs")
set(LLVM_USE_LINKER lld CACHE STRING "Linker to use")

set(CMAKE_INSTALL_PREFIX $ENV{LLVM_INSTALL} CACHE PATH "Install prefix")
set(LLVM_CCACHE_BUILD ON CACHE BOOL "Use ccache")

# Set Spack view paths from environment
set(SPACK_VIEW_PATH "$ENV{PROJECTS_DIR}/iara/spack/var/spack/environments/iara_env/.spack-env/view")
if(EXISTS ${SPACK_VIEW_PATH})
  set(CMAKE_INCLUDE_PATH "${SPACK_VIEW_PATH}/include" CACHE PATH "Additional include path")
  set(CMAKE_LIBRARY_PATH "${SPACK_VIEW_PATH}/lib" CACHE PATH "Additional library path")
  message(STATUS "Using Spack view at: ${SPACK_VIEW_PATH}")
else()
  message(WARNING "Spack view not found at: ${SPACK_VIEW_PATH}")
endif()

# Commented out to match LLVM/MLIR library build configuration
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -v -L${SPACK_VIEW_PATH}/lib -Wl,--disable-new-dtags -Wl,-rpath,${SPACK_VIEW_PATH}/lib ${SPACK_VIEW_PATH}/lib/libstdc++.so.6")

# add_compile_definitions("IARA_DEBUG")
add_compile_definitions("IARA_COMPILER")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  find_package(MLIR REQUIRED CONFIG)

  message(STATUS "Using MLIRConfig.cmake in: ${MLIR_DIR}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

  set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/bin)
  set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/lib)
  set(MLIR_BINARY_DIR ${CMAKE_BINARY_DIR})

  list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
  list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

  include(TableGen)
  include(AddLLVM)
  include(AddMLIR)
  include(HandleLLVMOptions)
else()
  # Build via external projects mechanism
  set(MLIR_MAIN_SRC_DIR ${LLVM_MAIN_SRC_DIR}/../mlir)
  set(MLIR_INCLUDE_DIR ${MLIR_MAIN_SRC_DIR}/include)
  set(MLIR_GENERATED_INCLUDE_DIR ${LLVM_BINARY_DIR}/tools/mlir/include)
  set(MLIR_INCLUDE_DIRS "${MLIR_INCLUDE_DIR};${MLIR_GENERATED_INCLUDE_DIR}")
endif()

if(MLIR_ENABLE_BINDINGS_PYTHON)
  include(MLIRDetectPythonEnv)
  mlir_configure_python_dev_packages()
endif()

set(IARA_SOURCE_DIR ${PROJECT_SOURCE_DIR})
set(IARA_BINARY_DIR ${PROJECT_BINARY_DIR})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
include_directories(SYSTEM ${MLIR_INCLUDE_DIRS})
include_directories(SYSTEM ${IARA_SOURCE_DIR}/external)
include_directories(${IARA_SOURCE_DIR}/include)
include_directories(${IARA_BINARY_DIR}/include)
link_directories(${LLVM_BUILD_LIBRARY_DIR})
add_definitions(${LLVM_DEFINITIONS})

include(CTest)
enable_testing()

add_subdirectory(include)
add_subdirectory(lib)
if(MLIR_ENABLE_BINDINGS_PYTHON)

  message(STATUS "Enabling Python API")
  add_subdirectory(python)
endif()

# Build iara-opt compiler (always included)
add_subdirectory(iara-opt)

# Include pre-generated test CMakeLists.txt files from applications
# (These are generated by scripts/generate_experiments.py during build_tests.sh Step 1)
if(IARA_BUILD_TESTS)
  message(STATUS "Including pre-generated experiment tests...")

  file(GLOB app_cmakelists_files RELATIVE "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/applications/*/experiment/CMakeLists.txt"
  )

  foreach(cmake_file ${app_cmakelists_files})
    message(STATUS "Including generated tests: ${cmake_file}")
    include(${CMAKE_SOURCE_DIR}/${cmake_file})
  endforeach()
endif()

string(REPLACE "-fno-lifetime-dse" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
