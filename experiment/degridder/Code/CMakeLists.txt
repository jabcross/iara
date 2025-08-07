cmake_minimum_required(VERSION 3.10.2)

# *******************************************
# ************* CMake Content ***************
# *******************************************
# This CMake create a workspace containing the following projects
#
# Program
#  - degridder


# Use custom Clang/LLVM compiler
set(CMAKE_C_COMPILER ${LLVM_INSTALL}/bin/clang)
set(CMAKE_CXX_COMPILER ${LLVM_INSTALL}/bin/clang++)

# Add definition for relative path into project
add_definitions( -DPROJECT_ROOT_PATH="${CMAKE_CURRENT_SOURCE_DIR}")
add_compile_definitions(IARA_USE_MUTEX_HASHMAP=1 IARA_DISABLE_OMP=1)

project(degridder C CXX)

# Set to 1 to activate debug compilation (0 for release)
if(NOT ${CMAKE_GENERATOR} MATCHES "Visual Studio.*")
    if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
        MESSAGE("Generate Debug project")
        #			set(CMAKE_BUILD_TYPE Debug)
        #			set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Debug)
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -pg -Wall ${ADDITIONAL_C_FLAGS}")
    else()
        MESSAGE("Generate Release project")
        #			set(CMAKE_BUILD_TYPE Release)
        #			set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release)
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g -pg -Wall ${ADDITIONAL_C_FLAGS}")
    endif()

    #add libmath during non visual studio builds
    set(CMAKE_EXTRA_LIB m)
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

set(LIBS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/lib/cmake_modules/")


# *******************************************
# ************ Pthread LIBRARY **************
# *******************************************
if (WIN32)
    set(THREADS_USE_PTHREADS_WIN32 true)
    # pthread included AFTER Sdl to avoid unnecessary warnings
    file(GLOB PTHREADDIR "${LIBS_DIR}/pthread-[\\.|0-9]*")
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${PTHREADDIR})
    Find_Package ( THREADS REQUIRED )
else()
    Find_Package ( Threads )
endif()

if(NOT THREADS_FOUND)
    MESSAGE(FATAL_ERROR "Pthread not found !")
endif()


if (WIN32)
    file(GLOB
            PTHREADS_DLL
            ${CMAKE_PREFIX_PATH}/lib/*.dll
            )

    MESSAGE("Copy Pthreads DLLs into ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    if(NOT ${CMAKE_GENERATOR} MATCHES "Visual Studio.*")
        file(COPY ${PTHREADS_DLL} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    else()
        file(COPY ${PTHREADS_DLL} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug)
        file(COPY ${PTHREADS_DLL} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release)
    endif()
endif()

# *******************************************
# ************ OpenMP LIBRARY ***************
# *******************************************
OPTION (USE_OpenMP "Use OpenMP" ON)
IF(USE_OpenMP)
    FIND_PACKAGE(OpenMP)
    IF(OPENMP_FOUND)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    ENDIF()
ENDIF()

# *******************************************
# ************ FFTW LIBRARY *****************
# *******************************************

#set (FFTW_INCLUDE_DIRS /usr/lib/x86_64-linux-gnu /usr/include/)
#Find_Package (FFTW)
#if(NOT FFTW_FOUND)
#    MESSAGE(FATAL_ERROR "FFTW not found !")
#endif()


# *******************************************
# ************ Lapack LIBRARY **************
# *******************************************

#SET(ENV{MKLROOT} /opt/intel/mkl)
#set(BLA_VENDOR Intel10_64lp)
#set(BLA_VENDOR Generic)

#Find_Package(LAPACK REQUIRED)
#if(NOT LAPACK_FOUND)
#    MESSAGE(FATAL_ERROR "LAPACK not found !")
#endif()

# *******************************************
# ************ casacore LIBRARY **************
# *******************************************
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# # Check for required environment variable
# if(NOT DEFINED ENV{EXPERIMENT_INSTANCE})
#     message(FATAL_ERROR "Environment variable EXPERIMENT_INSTANCE is not set. Please set it before running CMake.")
# endif()

include_directories(/usr/local/include)
include_directories($ENV{INSTANCE_BUILD_DIR})
link_directories(/usr/local/lib)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -g -Wall -mcmodel=large")
#message("source_files_CPU : ${source_files_CPU}")
add_library(casa_wrapper STATIC
        src/casacore_wrapper.cpp
        include/casacore_wrapper.h
)
target_link_libraries(casa_wrapper casa_casa casa_tables casa_ms casa_measures casa_fits casa_mirlib -lfftw3)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcmodel=large")


# *******************************************
# ***** CPU Version  ***********
# *******************************************
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${THREADS_PTHREADS_INCLUDE_DIR} ${FFTW_INCLUDE_DIRS}
$ENV{IARA_DIR}/include $ENV{IARA_DIR}/external)

# Separate LLVM IR files from other sources
file(GLOB llvm_ir_files 
    $ENV{INSTANCE_BUILD_DIR}/*.ll
)

file(
        GLOB
        source_files_CPU
        # ./generated/*.c
        $ENV{IARA_DIR}/experiment/degridder/Code/src/*.c
        $ENV{IARA_DIR}/experiment/degridder/Code/src/*.cpp
        $ENV{IARA_DIR}/runtime/virtual-fifo/*.cpp
        $ENV{INSTANCE_BUILD_DIR}/*.cpp
        $ENV{INSTANCE_BUILD_DIR}/*.c
)

# Convert LLVM IR files to object files
set(llvm_obj_files "")
foreach(ir_file ${llvm_ir_files})
    get_filename_component(ir_name ${ir_file} NAME_WE)
    set(obj_file "${CMAKE_CURRENT_BINARY_DIR}/${ir_name}.o")
    
    add_custom_command(
        OUTPUT ${obj_file}
        COMMAND $ENV{LLVM_INSTALL}/bin/llc -filetype=obj ${ir_file} -o ${obj_file}
        DEPENDS ${ir_file}
        COMMENT "Compiling ${ir_file} to object file"
    )
    
    list(APPEND llvm_obj_files ${obj_file})
endforeach()

# Create a custom target for LLVM IR compilation
if(llvm_obj_files)
    add_custom_target(compile_llvm_ir ALL DEPENDS ${llvm_obj_files})
endif()

file(
        GLOB
        header_files
        $ENV{IARA_DIR}/experiment/degridder/Code/include/*.h
        $ENV{INSTANCE_BUILD_DIR}/*.h
)

message(STATUS "Source files for CPU: ${source_files_CPU}")
message(STATUS "IARA_DIR: $ENV{IARA_DIR}")

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -g -Wall")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-multiple-definition")

#message("source_files_CPU : ${source_files_CPU}")

add_executable( degridder_pipeline ${source_files_CPU} ${header_files} ${llvm_obj_files})

# Make sure the IR is compiled before the executable
if(llvm_obj_files)
    add_dependencies(degridder_pipeline compile_llvm_ir)
endif()


target_link_libraries(degridder_pipeline ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_EXTRA_LIB} fftw3 fftw3f lapack m casa_wrapper)
set_target_properties(degridder_pipeline PROPERTIES
    LINKER_LANGUAGE CXX
)
