cmake_minimum_required(VERSION 3.10.2)

# *******************************************
# ************* CMake Content ***************
# *******************************************
# This CMake create a workspace containing the following projects
#
# Program
#  - degridder


# Use custom Clang/LLVM compiler

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_C_COMPILER /home/jabcross/llvm-install/bin/clang)
set(CMAKE_CXX_COMPILER /home/jabcross/llvm-install/bin/clang++)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Add definition for relative path into project
add_definitions(-DPROJECT_ROOT_PATH="${CMAKE_CURRENT_SOURCE_DIR}")

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

Find_Package(Threads)


if(NOT THREADS_FOUND)
    MESSAGE(FATAL_ERROR "Pthread not found !")
endif()


# *******************************************
# ************ OpenMP LIBRARY ***************
# *******************************************
OPTION(USE_OpenMP "Use OpenMP" ON)
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

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")


include_directories(/usr/local/include)
include_directories($ENV{IARA_DIR}/experiment/degridder/Code/include)
link_directories(/usr/local/lib)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -mcmodel=large")
#message("source_files_CPU : ${source_files_CPU}")
add_library(casa_wrapper STATIC
    $ENV{IARA_DIR}/experiment/degridder/Code/src/casacore_wrapper.cpp
)

target_link_libraries(casa_wrapper casa_casa casa_tables casa_ms casa_measures casa_fits casa_mirlib -lfftw3)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcmodel=large")

# *******************************************
# ***** CPU Version  ***********
# *******************************************
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/generated
    ${CMAKE_CURRENT_BINARY_DIR}
    ${THREADS_PTHREADS_INCLUDE_DIR}
    ${FFTW_INCLUDE_DIRS}
    $ENV{IARA_DIR}/include
    $ENV{IARA_DIR}/external
)

file(
    GLOB
    source_files_CPU
    $ENV{IARA_DIR}/experiment/degridder/Code/src/*.c
    ${CMAKE_CURRENT_BINARY_DIR}/main.cpp
    $ENV{IARA_DIR}/runtime/$ENV{SCHEDULER_MODE}/*.cpp
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-multiple-definition")

#message("source_files_CPU : ${source_files_CPU}")

add_executable(degridder_pipeline ${source_files_CPU} schedule.o)

target_link_libraries(degridder_pipeline ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_EXTRA_LIB} fftw3 fftw3f lapack m casa_wrapper omp pthread)
