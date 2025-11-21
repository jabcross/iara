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

# Define custom build types
# Available build types:
#   Debug               - Standard debug build with symbols
#   Release             - Standard optimized release build
#   CompilerDebug       - For iara-opt: Debug with -ftime-trace enabled
#   TestDebug           - For tests: Debug build for testing
#   ExperimentSize      - For experiments: Optimize for minimal binary size
#   ExperimentPerf      - For experiments: Optimize for maximum performance

# CompilerDebug: Debug build with profiling for iara-opt
set(CMAKE_CXX_FLAGS_COMPILERDEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ftime-trace" CACHE STRING "Compiler Debug CXX flags" FORCE)
set(CMAKE_C_FLAGS_COMPILERDEBUG "${CMAKE_C_FLAGS_DEBUG}" CACHE STRING "Compiler Debug C flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_COMPILERDEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG}" CACHE STRING "Compiler Debug linker flags" FORCE)

# TestDebug: Debug build for tests
set(CMAKE_CXX_FLAGS_TESTDEBUG "${CMAKE_CXX_FLAGS_DEBUG}" CACHE STRING "Test Debug CXX flags" FORCE)
set(CMAKE_C_FLAGS_TESTDEBUG "${CMAKE_C_FLAGS_DEBUG}" CACHE STRING "Test Debug C flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_TESTDEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG}" CACHE STRING "Test Debug linker flags" FORCE)

# ExperimentSize: Optimize for minimal binary size
set(CMAKE_CXX_FLAGS_EXPERIMENTSIZE "-Os -DNDEBUG -fvisibility=hidden -fno-rtti -fno-exceptions -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections" CACHE STRING "Experiment Size CXX flags" FORCE)
set(CMAKE_C_FLAGS_EXPERIMENTSIZE "-Os -DNDEBUG -fvisibility=hidden -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections" CACHE STRING "Experiment Size C flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_EXPERIMENTSIZE "-Wl,--gc-sections -flto" CACHE STRING "Experiment Size linker flags" FORCE)

# ExperimentPerf: Optimize for maximum performance
set(CMAKE_CXX_FLAGS_EXPERIMENTPERF "-O3 -DNDEBUG -march=native -mtune=native -fno-rtti -fno-exceptions -flto" CACHE STRING "Experiment Perf CXX flags" FORCE)
set(CMAKE_C_FLAGS_EXPERIMENTPERF "-O3 -DNDEBUG -march=native -mtune=native -flto" CACHE STRING "Experiment Perf C flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_EXPERIMENTPERF "-flto" CACHE STRING "Experiment Perf linker flags" FORCE)

# ExperimentPerf: Enable debug data for profiling
set(CMAKE_CXX_FLAGS_EXPERIMENTPROF "-O3 ${CMAKE_CXX_FLAGS_DEBUG} -gdwarf -march=native -mtune=native -fno-exceptions -flto" CACHE STRING "Experiment Perf CXX flags" FORCE)
set(CMAKE_C_FLAGS_EXPERIMENTPROF "-O3 ${CMAKE_C_FLAGS_DEBUG} -gdwarf -march=native -mtune=native -flto" CACHE STRING "Experiment Perf C flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_EXPERIMENTPROF "-flto" CACHE STRING "Experiment Perf linker flags" FORCE)

# Mark custom build types as advanced
mark_as_advanced(
  CMAKE_CXX_FLAGS_COMPILERDEBUG
  CMAKE_C_FLAGS_COMPILERDEBUG
  CMAKE_EXE_LINKER_FLAGS_COMPILERDEBUG
  CMAKE_CXX_FLAGS_TESTDEBUG
  CMAKE_C_FLAGS_TESTDEBUG
  CMAKE_EXE_LINKER_FLAGS_TESTDEBUG
  CMAKE_CXX_FLAGS_EXPERIMENTSIZE
  CMAKE_C_FLAGS_EXPERIMENTSIZE
  CMAKE_EXE_LINKER_FLAGS_EXPERIMENTSIZE
  CMAKE_CXX_FLAGS_EXPERIMENTPERF
  CMAKE_C_FLAGS_EXPERIMENTPERF
  CMAKE_EXE_LINKER_FLAGS_EXPERIMENTPERF
)

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
  "Debug" "Release" "CompilerDebug" "TestDebug" "ExperimentSize" "ExperimentPerf")


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
add_subdirectory(test)
add_subdirectory(iara-opt)

string(REPLACE "-fno-lifetime-dse" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
