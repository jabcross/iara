include(CTest)

# Disable ccache for test builds to ensure build-type flags are properly applied
# This is important for size optimization experiments where assertions must be disabled
set(CMAKE_C_COMPILER_LAUNCHER "")
set(CMAKE_CXX_COMPILER_LAUNCHER "")

# Scheduler modes to test; only set defaults if the variable isn't already set
if(NOT DEFINED SCHEDULER_MODES OR "${SCHEDULER_MODES}" STREQUAL "")
        set(SCHEDULER_MODES "virtual-fifo")
endif()
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

function(iara_runtime_sources iara_opt_sched runtime_backend out_var out_compile_defs)
        # Determine which runtime sources to link based on IARA_OPT_SCHEDULER and IARA_RUNTIME_BACKEND
        # These come from the test's parameters.sh file

        set(srcs "")
        set(compile_defs "")

        if("${iara_opt_sched}" STREQUAL "virtual-fifo")
                # Virtual FIFO runtime sources
                set(srcs
                        ${PROJECT_SOURCE_DIR}/runtime/virtual-fifo/Common.cpp
                        ${PROJECT_SOURCE_DIR}/runtime/virtual-fifo/ScatterGather.cpp
                        ${PROJECT_SOURCE_DIR}/runtime/virtual-fifo/VirtualFIFO_Chunk.cpp
                        ${PROJECT_SOURCE_DIR}/runtime/virtual-fifo/VirtualFIFO_Edge.cpp
                        ${PROJECT_SOURCE_DIR}/runtime/virtual-fifo/VirtualFIFO_Node.cpp
                        ${PROJECT_SOURCE_DIR}/runtime/virtual-fifo/VirtualFIFO_Scheduler.cpp
                )

                # Add backend-specific sources
                if("${runtime_backend}" STREQUAL "enkits")
                        list(APPEND srcs
                                ${PROJECT_SOURCE_DIR}/runtime/common/WorkStealingBackend_EnkiTS.cpp
                                ${PROJECT_SOURCE_DIR}/external/enkiTS/TaskScheduler.cpp
                        )
                        set(compile_defs "IARA_PARALLELISM_ENKITS")
                else()
                        # Default to OpenMP
                        set(compile_defs "IARA_PARALLELISM_OMP")
                endif()

        elseif("${iara_opt_sched}" STREQUAL "ring-buffer")
                set(srcs
                        ${PROJECT_SOURCE_DIR}/runtime/ring-buffer/MutexRingBuffer.cpp
                        ${PROJECT_SOURCE_DIR}/runtime/ring-buffer/RingBuffer_Edge.cpp
                        ${PROJECT_SOURCE_DIR}/runtime/ring-buffer/RingBuffer_Node.cpp
                        ${PROJECT_SOURCE_DIR}/runtime/ring-buffer/RingBuffer_Scheduler.cpp
                )
                set(compile_defs "")
        endif()

        set(${out_var} ${srcs} PARENT_SCOPE)
        set(${out_compile_defs} ${compile_defs} PARENT_SCOPE)
endfunction()

function(iara_add_runtime_test entry scheduler)
        set(test_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/Iara/${entry}")
        if(NOT IS_DIRECTORY "${test_src_dir}")
                return()
        endif()

        set(base "${entry}-${scheduler}")
        message(STATUS "Configuring IaRa test ${base}")

        set(test_binary_root "${CMAKE_CURRENT_BINARY_DIR}/Iara/${entry}")
        set(build_subdir "${test_binary_root}/build-${scheduler}")

        # Check for centralized application sources first
        # Extract application name from test directory (e.g., "05-cholesky" -> "cholesky", "05.1-cholesky-10080_10_vf-omp" -> "cholesky")
        string(REGEX REPLACE "^[0-9.]*-([^-]+).*" "\\1" app_name "${entry}")
        set(app_src_dir "${PROJECT_SOURCE_DIR}/applications/${app_name}")

        # Try to find sources in applications folder first, then fall back to test directory
        if(IS_DIRECTORY "${app_src_dir}")
                file(GLOB c_sources CONFIGURE_DEPENDS "${app_src_dir}/*.c")
                file(GLOB cpp_sources CONFIGURE_DEPENDS "${app_src_dir}/*.cpp")
                set(app_include_dir "${app_src_dir}")
        else()
                file(GLOB c_sources CONFIGURE_DEPENDS "${test_src_dir}/*.c")
                file(GLOB cpp_sources CONFIGURE_DEPENDS "${test_src_dir}/*.cpp")
                set(app_include_dir "${test_src_dir}")
        endif()

        if(NOT c_sources AND NOT cpp_sources)
                message(STATUS "Skipping IaRa test ${base} (no C/C++ sources found)")
                return()
        endif()

        # Check if this is a baseline scheduler (no IaRa runtime)
        set(is_baseline_scheduler FALSE)
        if("${scheduler}" STREQUAL "sequential" OR "${scheduler}" STREQUAL "omp-task" OR "${scheduler}" STREQUAL "omp-for" OR "${scheduler}" STREQUAL "enkits-task")
                set(is_baseline_scheduler TRUE)
        endif()

        # Get build-type-specific flags (e.g., CMAKE_CXX_FLAGS_EXPERIMENTSIZE contains -DNDEBUG)
        # These flags will be applied to all compilation: schedule.o, runtime sources, and test sources
        string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type_upper)
        set(build_type_cxx_flags "${CMAKE_CXX_FLAGS_${build_type_upper}}")
        separate_arguments(build_type_cxx_flags)

        # Map test scheduler to IARA components:
        # - IARA_OPT_SCHEDULER: scheduler algorithm for iara-opt (virtual-fifo or ring-buffer)
        # - IARA_RUNTIME_BACKEND: runtime parallelism backend (omp or enkits)
        # - SCHEDULER_FLAG: compile flag for main.c (-DSCHEDULER_xxx)

        set(IARA_OPT_SCHEDULER "")
        set(IARA_RUNTIME_BACKEND "")
        set(SCHEDULER_FLAG "")

        if("${scheduler}" STREQUAL "virtual-fifo" OR "${scheduler}" STREQUAL "vf-omp")
                set(IARA_OPT_SCHEDULER "virtual-fifo")
                set(IARA_RUNTIME_BACKEND "omp")
                set(SCHEDULER_FLAG "-DSCHEDULER_IARA")
        elseif("${scheduler}" STREQUAL "vf-enkits")
                set(IARA_OPT_SCHEDULER "virtual-fifo")
                set(IARA_RUNTIME_BACKEND "enkits")
                set(SCHEDULER_FLAG "-DSCHEDULER_IARA")
        elseif("${scheduler}" STREQUAL "ring-buffer")
                set(IARA_OPT_SCHEDULER "ring-buffer")
                set(IARA_RUNTIME_BACKEND "omp")
                set(SCHEDULER_FLAG "-DSCHEDULER_IARA")
        elseif("${scheduler}" STREQUAL "sequential")
                set(SCHEDULER_FLAG "-DSCHEDULER_SEQUENTIAL")
        elseif("${scheduler}" STREQUAL "omp-for")
                set(SCHEDULER_FLAG "-DSCHEDULER_OMP_FOR")
        elseif("${scheduler}" STREQUAL "omp-task")
                set(SCHEDULER_FLAG "-DSCHEDULER_OMP_TASK")
        elseif("${scheduler}" STREQUAL "enkits-task")
                set(SCHEDULER_FLAG "-DSCHEDULER_ENKITS_TASK")
        endif()

        # Source parameters.sh to get IARA_OPT_SCHEDULER and IARA_RUNTIME_BACKEND
        set(parameters_script "")
        if(EXISTS "${test_src_dir}/parameters.sh")
                set(parameters_script "${test_src_dir}/parameters.sh")
        endif()

        # Read IARA configuration from parameters.sh if it exists
        set(iara_opt_from_params "")
        set(runtime_backend_from_params "")
        if(parameters_script)
                execute_process(
                        COMMAND ${BASH_EXECUTABLE} -c "source ${parameters_script} && echo $IARA_OPT_SCHEDULER"
                        OUTPUT_VARIABLE iara_opt_from_params
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                )
                execute_process(
                        COMMAND ${BASH_EXECUTABLE} -c "source ${parameters_script} && echo $IARA_RUNTIME_BACKEND"
                        OUTPUT_VARIABLE runtime_backend_from_params
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                )
        endif()

        # Use parameters from file if available, otherwise use mapped values
        if(iara_opt_from_params)
                set(final_iara_opt ${iara_opt_from_params})
                set(final_runtime_backend ${runtime_backend_from_params})
        else()
                set(final_iara_opt ${IARA_OPT_SCHEDULER})
                set(final_runtime_backend ${IARA_RUNTIME_BACKEND})
        endif()

        # Only get runtime sources if this is an IaRa scheduler (not baseline)
        set(runtime_sources "")
        set(runtime_compile_defs "")
        if(NOT is_baseline_scheduler AND final_iara_opt)
                iara_runtime_sources(${final_iara_opt} ${final_runtime_backend} runtime_sources runtime_compile_defs)
        endif()

        if(NOT is_baseline_scheduler AND final_iara_opt)
                iara_collect_args(iara_flags "IARA_FLAGS" --iara-canonicalize --flatten --${final_iara_opt}=main-actor=run)
        endif()
        iara_collect_args(schedule_extra_args "EXTRA_SCHEDULE_ARGS")
        iara_collect_args(kernel_extra_args "EXTRA_KERNEL_ARGS")
        iara_collect_args(runtime_extra_args "EXTRA_RUNTIME_ARGS")
        iara_collect_args(link_extra_args "EXTRA_LINKER_ARGS")

        set(codegen_stamp "")
        # Look for codegen.sh in app directory (centralized) or test directory (legacy)
        set(codegen_script "")
        if(EXISTS "${app_include_dir}/codegen.sh")
                set(codegen_script "${app_include_dir}/codegen.sh")
        elseif(EXISTS "${test_src_dir}/codegen.sh")
                set(codegen_script "${test_src_dir}/codegen.sh")
        endif()

        if(codegen_script)
                # Read NUM_BLOCKS and MATRIX_SIZE from parameters.sh if available
                set(num_blocks_val "")
                set(matrix_size_val "")
                if(parameters_script)
                        execute_process(
                                COMMAND ${BASH_EXECUTABLE} -c "source ${parameters_script} && echo $NUM_BLOCKS"
                                OUTPUT_VARIABLE num_blocks_val
                                OUTPUT_STRIP_TRAILING_WHITESPACE
                        )
                        execute_process(
                                COMMAND ${BASH_EXECUTABLE} -c "source ${parameters_script} && echo $MATRIX_SIZE"
                                OUTPUT_VARIABLE matrix_size_val
                                OUTPUT_STRIP_TRAILING_WHITESPACE
                        )
                endif()

                # Use values from parameters.sh if available, otherwise fall back to environment or defaults
                if(num_blocks_val)
                        set(final_num_blocks ${num_blocks_val})
                elseif(DEFINED ENV{NUM_BLOCKS} AND NOT "$ENV{NUM_BLOCKS}" STREQUAL "")
                        set(final_num_blocks $ENV{NUM_BLOCKS})
                else()
                        set(final_num_blocks 4)
                endif()

                if(matrix_size_val)
                        set(final_matrix_size ${matrix_size_val})
                elseif(DEFINED ENV{MATRIX_SIZE} AND NOT "$ENV{MATRIX_SIZE}" STREQUAL "")
                        set(final_matrix_size $ENV{MATRIX_SIZE})
                else()
                        set(final_matrix_size 2048)
                endif()

                set(codegen_stamp "${build_subdir}/codegen.stamp")
                add_custom_command(
                        OUTPUT ${codegen_stamp}
                        COMMAND ${CMAKE_COMMAND} -E make_directory ${build_subdir}
                        COMMAND ${CMAKE_COMMAND} -E env
                        PATH_TO_APP_SOURCES=${app_include_dir}
                        PATH_TO_TEST_SOURCES=${test_src_dir}
                        PATH_TO_TEST_BUILD_DIR=${build_subdir}
                        NUM_BLOCKS=${final_num_blocks}
                        MATRIX_SIZE=${final_matrix_size}
                        TEST_SCHEDULER=${scheduler}
                        IARA_OPT_SCHEDULER=${final_iara_opt}
                        IARA_RUNTIME_BACKEND=${final_runtime_backend}
                        ${BASH_EXECUTABLE} ${codegen_script}
                        COMMAND ${CMAKE_COMMAND} -E touch ${codegen_stamp}
                        DEPENDS ${codegen_script}
                        WORKING_DIRECTORY ${build_subdir}
                        COMMENT "Running codegen for ${base}"
                        VERBATIM
                )

                # Add scheduler compile flag and other flags
                if(SCHEDULER_FLAG)
                        list(APPEND kernel_extra_args "${SCHEDULER_FLAG}")
                endif()
                list(APPEND kernel_extra_args "-DNUM_BLOCKS=${final_num_blocks}" "-DMATRIX_SIZE=${final_matrix_size}" "-DVERBOSE")
                list(APPEND link_extra_args "-lopenblas")
        endif()


        set(iara_opt_binary "$ENV{IARA_DIR}/build/bin/iara-opt")
        if(NOT iara_opt_binary)
                message(FATAL_ERROR "Unable to find iara-opt. Please build iara-opt in the main project and/or set IARA_OPT_PATH to its path.")
        endif()

        # For baseline schedulers, skip schedule generation entirely
        set(schedule_obj "")
        if(NOT is_baseline_scheduler)
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

                set(schedule_deps "")
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
                        ${iara_opt_binary} ${iara_flags} ${topology_input} > ${schedule_mlir}
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
                        ${build_type_cxx_flags}
                        ${IARA_BUILD_OPT}
                        -stdlib=libc++
                        ${OpenMP_CXX_FLAGS}
                        ${schedule_extra_args}
                        ${schedule_include_args}
                        -xir
                        -c
                        ${schedule_ll}
                        -o
                        ${schedule_obj}
                )

                # Create the schedule.o as a custom command (needed because it's LLVM IR)
                add_custom_command(
                        OUTPUT ${schedule_obj}
                        COMMAND ${CMAKE_COMMAND} -E env LLVM_INSTALL=$ENV{LLVM_INSTALL} ${CMAKE_CXX_COMPILER} ${schedule_compile_args}
                        DEPENDS ${schedule_ll}
                        WORKING_DIRECTORY ${build_subdir}
                        COMMENT "Compiling schedule.ll for ${base}"
                        COMMAND_EXPAND_LISTS
                        VERBATIM
                )
        endif()

        set(target_name "run-${base}")

        # Build list of sources for executable
        set(exec_sources ${c_sources} ${cpp_sources} ${runtime_sources})
        if(schedule_obj)
                list(APPEND exec_sources ${schedule_obj})
        endif()

        add_executable(${target_name} ${exec_sources})

        if(schedule_obj)
                set_source_files_properties(${schedule_obj} PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
        endif()

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
                ${PROJECT_SOURCE_DIR}/external/enkiTS/src
                ${app_include_dir}
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
        list(APPEND common_compile_options ${build_type_cxx_flags})
        list(APPEND common_compile_options ${IARA_BUILD_OPT})
        list(APPEND common_compile_options ${kernel_extra_args})
        list(APPEND common_compile_options ${runtime_extra_args})
        if(runtime_compile_defs)
                list(APPEND common_compile_options "-D${runtime_compile_defs}")
        endif()

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
        target_link_options(${target_name} PRIVATE ${OpenMP_CXX_FLAGS} -stdlib=libc++ -Wl,--gc-sections)
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

        # # Strip symbols from the binary to minimize size
        # add_custom_command(TARGET ${target_name} POST_BUILD
        #         COMMAND ${CMAKE_COMMAND} -E echo "Stripping symbols from ${build_subdir}/a.out"
        #         COMMAND strip ${build_subdir}/a.out
        #         COMMENT "Stripping ${base} binary"
        # )

        add_custom_target(build-${base} DEPENDS ${target_name})
        string(JOIN " " iara_flags_str ${iara_flags})
        set(iara_opt_path ${iara_opt_binary})
        # Build the test target as the "build-..." test so that the subsequent
        # "run-..." test has the executable (a.out) available in the
        # test's build directory. Previously this test only invoked iara-opt
        # which didn't actually compile the C/C++ test binary.
        add_test(
                NAME build-${base}
                COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target build-${base}
        )
        set_tests_properties(build-${base} PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR} FIXTURES_SETUP build-${base})

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
                # Check if test directory name encodes a specific scheduler
                # e.g., "05.1-cholesky-10080_10_vf-omp" -> should only build with vf-omp
                set(specific_scheduler "")
                if("${entry}" MATCHES "_vf-omp$")
                        set(specific_scheduler "vf-omp")
                elseif("${entry}" MATCHES "_vf-enkits$")
                        set(specific_scheduler "vf-enkits")
                elseif("${entry}" MATCHES "_omp-for$")
                        set(specific_scheduler "omp-for")
                elseif("${entry}" MATCHES "_omp-task$")
                        set(specific_scheduler "omp-task")
                elseif("${entry}" MATCHES "_enkits-task$")
                        set(specific_scheduler "enkits-task")
                elseif("${entry}" MATCHES "_sequential$")
                        set(specific_scheduler "sequential")
                endif()

                if(specific_scheduler)
                        # Test has a specific scheduler in its name - only build with that
                        iara_add_runtime_test(${entry} ${specific_scheduler})
                else()
                        # No specific scheduler in name - build with all schedulers in SCHEDULER_MODES
                        foreach(scheduler IN LISTS SCHEDULER_MODES)
                                iara_add_runtime_test(${entry} ${scheduler})
                        endforeach()
                endif()
        endif()
endforeach()
