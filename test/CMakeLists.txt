include(CTest)

# Scheduler modes to test
set(SCHEDULER_MODES "virtual-fifo")
# set(SCHEDULER_MODES "virtual-fifo" "ring-buffer")

if(NOT DEFINED ENV{LLVM_INSTALL})
        message(FATAL_ERROR "LLVM_INSTALL environment variable not set. Please configure the IaRa environment before building tests.")
endif()

if(NOT DEFINED ENV{IARA_DIR} OR "$ENV{IARA_DIR}" STREQUAL "")
        set(ENV{IARA_DIR} "${PROJECT_SOURCE_DIR}")
endif()

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
        message(FATAL_ERROR "Failed to locate libomp. Ensure LLVM's OpenMP runtime is installed.")
endif()

get_filename_component(OpenMP_CXX_LIBRARY_DIR "${OpenMP_CXX_LIBRARY}" DIRECTORY)

find_program(BASH_EXECUTABLE bash REQUIRED)

execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} --print-resource-dir
        OUTPUT_VARIABLE IARA_CLANG_RESOURCE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT IARA_CLANG_RESOURCE_DIR)
        message(FATAL_ERROR "Failed to query clang resource directory from ${CMAKE_CXX_COMPILER}")
endif()

set(IARA_CLANG_INCLUDE_DIR "${IARA_CLANG_RESOURCE_DIR}/include")

function(iara_collect_args output_var env_var)
        if(DEFINED ENV{${env_var}} AND NOT "$ENV{${env_var}}" STREQUAL "")
                separate_arguments(parsed UNIX_COMMAND "$ENV{${env_var}}")
                set(${output_var} ${parsed} PARENT_SCOPE)
        else()
                set(${output_var} ${ARGN} PARENT_SCOPE)
        endif()
endfunction()

function(iara_runtime_sources scheduler out_var)
        if("${scheduler}" STREQUAL "virtual-fifo")
                set(srcs
                        ${PROJECT_SOURCE_DIR}/runtime/virtual-fifo/Common.cpp
                        ${PROJECT_SOURCE_DIR}/runtime/virtual-fifo/DistributeGather.cpp
                        ${PROJECT_SOURCE_DIR}/runtime/virtual-fifo/VirtualFIFO_Chunk.cpp
                        ${PROJECT_SOURCE_DIR}/runtime/virtual-fifo/VirtualFIFO_Edge.cpp
                        ${PROJECT_SOURCE_DIR}/runtime/virtual-fifo/VirtualFIFO_Node.cpp
                        ${PROJECT_SOURCE_DIR}/runtime/virtual-fifo/VirtualFIFO_Scheduler.cpp
                )
        elseif("${scheduler}" STREQUAL "ring-buffer")
                set(srcs
                        ${PROJECT_SOURCE_DIR}/runtime/ring-buffer/MutexRingBuffer.cpp
                        ${PROJECT_SOURCE_DIR}/runtime/ring-buffer/RingBuffer_Edge.cpp
                        ${PROJECT_SOURCE_DIR}/runtime/ring-buffer/RingBuffer_Node.cpp
                        ${PROJECT_SOURCE_DIR}/runtime/ring-buffer/RingBuffer_Scheduler.cpp
                )
        else()
                message(FATAL_ERROR "Unknown scheduler mode '${scheduler}'")
        endif()
        set(${out_var} ${srcs} PARENT_SCOPE)
endfunction()

if(DEFINED ENV{BUILD_OPTIMIZATION} AND NOT "$ENV{BUILD_OPTIMIZATION}" STREQUAL "")
        separate_arguments(IARA_BUILD_OPT UNIX_COMMAND "$ENV{BUILD_OPTIMIZATION}")
else()
        set(IARA_BUILD_OPT "-g")
endif()

function(iara_add_runtime_test entry scheduler)
        set(test_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/Iara/${entry}")
        if(NOT IS_DIRECTORY "${test_src_dir}")
                return()
        endif()

        set(base "${entry}-${scheduler}")
        message(STATUS "Configuring IaRa test ${base}")

        set(test_binary_root "${CMAKE_CURRENT_BINARY_DIR}/Iara/${entry}")
        set(build_subdir "${test_binary_root}/build-${scheduler}")

        file(GLOB c_sources CONFIGURE_DEPENDS "${test_src_dir}/*.c")
        file(GLOB cpp_sources CONFIGURE_DEPENDS "${test_src_dir}/*.cpp")

        if(NOT c_sources AND NOT cpp_sources)
                message(STATUS "Skipping IaRa test ${base} (no C/C++ sources found)")
                return()
        endif()

        iara_runtime_sources(${scheduler} runtime_sources)

        iara_collect_args(iara_flags "IARA_FLAGS" --iara-canonicalize --flatten --${scheduler}=main-actor=run)
        iara_collect_args(schedule_extra_args "EXTRA_SCHEDULE_ARGS")
        iara_collect_args(kernel_extra_args "EXTRA_KERNEL_ARGS")
        iara_collect_args(runtime_extra_args "EXTRA_RUNTIME_ARGS")
        iara_collect_args(link_extra_args "EXTRA_LINKER_ARGS")

        set(codegen_stamp "")
        if(EXISTS "${test_src_dir}/codegen.sh")
                if(NOT DEFINED ENV{NUM_BLOCKS} OR "$ENV{NUM_BLOCKS}" STREQUAL "")
                        set(ENV{NUM_BLOCKS} 4)
                endif()
                if(NOT DEFINED ENV{MATRIX_SIZE} OR "$ENV{MATRIX_SIZE}" STREQUAL "")
                        set(ENV{MATRIX_SIZE} 2048)
                endif()

                set(codegen_stamp "${build_subdir}/codegen.stamp")
                add_custom_command(
                        OUTPUT ${codegen_stamp}
                        COMMAND ${CMAKE_COMMAND} -E make_directory ${build_subdir}
                        COMMAND ${CMAKE_COMMAND} -E env
                        PATH_TO_TEST_SOURCES=${test_src_dir}
                        PATH_TO_TEST_BUILD_DIR=${build_subdir}
                        NUM_BLOCKS=$ENV{NUM_BLOCKS}
                        MATRIX_SIZE=$ENV{MATRIX_SIZE}
                        SCHEDULER_MODE=${scheduler}
                        ${BASH_EXECUTABLE} ${test_src_dir}/codegen.sh
                        COMMAND ${CMAKE_COMMAND} -E touch ${codegen_stamp}
                        DEPENDS ${test_src_dir}/codegen.sh
                        WORKING_DIRECTORY ${build_subdir}
                        COMMENT "Running codegen for ${base}"
                        VERBATIM
                )

                list(APPEND kernel_extra_args "-DNUM_BLOCKS=$ENV{NUM_BLOCKS}" "-DMATRIX_SIZE=$ENV{MATRIX_SIZE}" "-DSCHEDULER_IARA" "-DVERBOSE")
                list(APPEND link_extra_args "-lopenblas")
        endif()

        set(topology_input "")
        if(DEFINED ENV{TOPOLOGY_FILE} AND NOT "$ENV{TOPOLOGY_FILE}" STREQUAL "")
                set(topology_input "$ENV{TOPOLOGY_FILE}")
        elseif(EXISTS "${test_src_dir}/topology.mlir")
                set(topology_input "${test_src_dir}/topology.mlir")
        elseif(EXISTS "${test_src_dir}/topology.dif")
                set(topology_input "${build_subdir}/topology.mlir")
        elseif(EXISTS "${test_src_dir}/topology.test")
                set(topology_input "${test_src_dir}/topology.test")
        else()
                set(topology_input "${build_subdir}/topology.mlir")
        endif()

        set(schedule_mlir "${build_subdir}/schedule.mlir")
        set(schedule_ll "${build_subdir}/schedule.ll")
        set(schedule_obj "${build_subdir}/schedule.o")

        set(schedule_deps iara-opt)
        if(codegen_stamp)
                list(APPEND schedule_deps ${codegen_stamp})
        endif()
        if(EXISTS "${topology_input}")
                list(APPEND schedule_deps "${topology_input}")
        endif()

        if(EXISTS "${test_src_dir}/topology.dif")
                add_custom_command(
                        OUTPUT ${build_subdir}/topology.mlir
                        COMMAND ${CMAKE_COMMAND} -E make_directory ${build_subdir}
                        COMMAND python3 ${PROJECT_SOURCE_DIR}/scripts/dif-to-iara.py ${test_src_dir}/topology.dif > ${build_subdir}/topology.mlir
                        DEPENDS ${test_src_dir}/topology.dif ${PROJECT_SOURCE_DIR}/scripts/dif-to-iara.py
                        COMMENT "Generating topology.mlir from topology.dif for ${base}"
                )
        endif()

        add_custom_command(
                OUTPUT ${schedule_mlir}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${build_subdir}
                COMMAND ${CMAKE_COMMAND} -E env
                IARA_DIR=$ENV{IARA_DIR}
                PATH_TO_TEST_SOURCES=${test_src_dir}
                PATH_TO_TEST_BUILD_DIR=${build_subdir}
                SCHEDULER_MODE=${scheduler}
                $<TARGET_FILE:iara-opt> ${iara_flags} ${topology_input} > ${schedule_mlir}
                DEPENDS ${schedule_deps}
                WORKING_DIRECTORY ${build_subdir}
                COMMENT "Generating schedule.mlir for ${base}"
                COMMAND_EXPAND_LISTS
                VERBATIM
        )

        add_custom_command(
                OUTPUT ${schedule_ll}
                COMMAND ${CMAKE_COMMAND} -E env LLVM_INSTALL=$ENV{LLVM_INSTALL} ${BASH_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/mlir-to-llvmir.sh ${schedule_mlir}
                DEPENDS ${schedule_mlir} ${PROJECT_SOURCE_DIR}/scripts/mlir-to-llvmir.sh
                WORKING_DIRECTORY ${build_subdir}
                COMMENT "Lowering schedule.mlir to LLVM IR for ${base}"
                VERBATIM
        )

        set(schedule_include_args
                -I${IARA_CLANG_INCLUDE_DIR}
                -I${build_subdir}
                -I${PROJECT_SOURCE_DIR}/include
                -I${PROJECT_SOURCE_DIR}/external
        )
        if(EXISTS "${test_src_dir}")
                list(APPEND schedule_include_args -I${test_src_dir})
        endif()
        foreach(optional_dir IN ITEMS ${test_src_dir}/include ${test_src_dir}/generated)
                if(EXISTS "${optional_dir}")
                        list(APPEND schedule_include_args -I${optional_dir})
                endif()
        endforeach()

        set(schedule_compile_args
                --std=c++20
                ${IARA_BUILD_OPT}
                -stdlib=libc++
                ${OpenMP_CXX_FLAGS}
                -ftime-trace=schedule.json
                ${schedule_extra_args}
                ${schedule_include_args}
                -xir
                -c
                ${schedule_ll}
                -o
                ${schedule_obj}
        )

        add_custom_command(
                OUTPUT ${schedule_obj}
                COMMAND ${CMAKE_COMMAND} -E env LLVM_INSTALL=$ENV{LLVM_INSTALL} ${CMAKE_CXX_COMPILER} ${schedule_compile_args}
                DEPENDS ${schedule_ll}
                WORKING_DIRECTORY ${build_subdir}
                COMMENT "Compiling schedule.ll for ${base}"
                COMMAND_EXPAND_LISTS
                VERBATIM
        )

        set(target_name "run-${base}")

        add_executable(${target_name}
                ${c_sources}
                ${cpp_sources}
                ${runtime_sources}
                ${schedule_obj}
        )

        set_source_files_properties(${schedule_obj} PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)

        set_target_properties(${target_name}
                PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY ${build_subdir}
                OUTPUT_NAME a
                SUFFIX ".out"
                C_STANDARD 11
                CXX_STANDARD 20
                CXX_STANDARD_REQUIRED ON
        )

        set(test_include_dirs
                ${PROJECT_SOURCE_DIR}/include
                ${PROJECT_SOURCE_DIR}/external
                ${test_src_dir}
                ${build_subdir}
        )
        foreach(extra_dir IN ITEMS ${test_src_dir}/include ${test_src_dir}/src ${test_src_dir}/generated)
                if(EXISTS "${extra_dir}")
                        list(APPEND test_include_dirs "${extra_dir}")
                endif()
        endforeach()

        target_include_directories(${target_name}
                PRIVATE
                ${test_include_dirs}
        )

        set(common_compile_options ${OpenMP_CXX_FLAGS})
        list(APPEND common_compile_options ${IARA_BUILD_OPT})
        list(APPEND common_compile_options ${kernel_extra_args})
        list(APPEND common_compile_options ${runtime_extra_args})

        target_compile_options(${target_name}
                PRIVATE
                $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>
                ${common_compile_options}
                -UIARA_COMPILER
        )

        target_link_directories(${target_name} PRIVATE $ENV{LLVM_INSTALL}/lib)
        if(OpenMP_CXX_LIBRARY_DIR)
                target_link_directories(${target_name} PRIVATE ${OpenMP_CXX_LIBRARY_DIR})
        endif()
        target_link_libraries(${target_name} PRIVATE ${OpenMP_CXX_LIBRARY} m pthread)
        target_link_options(${target_name} PRIVATE ${OpenMP_CXX_FLAGS} -stdlib=libc++)
        if(OpenMP_CXX_LIBRARY_DIR)
                target_link_options(${target_name} PRIVATE -Wl,-rpath,${OpenMP_CXX_LIBRARY_DIR})
        else()
                # If we couldn't detect the OpenMP library dir, add common LLVM lib paths as a fallback
                if(DEFINED ENV{LLVM_INSTALL} AND NOT "$ENV{LLVM_INSTALL}" STREQUAL "")
                        target_link_options(${target_name} PRIVATE -Wl,-rpath,$ENV{LLVM_INSTALL}/lib -Wl,-rpath,$ENV{LLVM_INSTALL}/lib/x86_64-unknown-linux-gnu)
                endif()
        endif()
        if(link_extra_args)
                target_link_options(${target_name} PRIVATE ${link_extra_args})
        endif()

        add_custom_target(build-${base} DEPENDS ${target_name})
        string(JOIN " " iara_flags_str ${iara_flags})
        set(iara_opt_path $<TARGET_FILE:iara-opt>)
        add_test(
                NAME build-${base}
                COMMAND ${iara_opt_path} ${iara_flags} ${topology_input}
        )
        set_tests_properties(build-${base} PROPERTIES WORKING_DIRECTORY ${build_subdir} FIXTURES_SETUP build-${base})

        add_test(
                NAME run-${base}
                COMMAND a.out
                WORKING_DIRECTORY ${build_subdir}
        )
        set_tests_properties(run-${base} PROPERTIES FIXTURES_REQUIRED build-${base})
endfunction()

file(GLOB entries RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/Iara" "${CMAKE_CURRENT_SOURCE_DIR}/Iara/*")
foreach(entry IN LISTS entries)
        if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Iara/${entry}")
                foreach(scheduler IN LISTS SCHEDULER_MODES)
                        iara_add_runtime_test(${entry} ${scheduler})
                endforeach()
        endif()
endforeach()
