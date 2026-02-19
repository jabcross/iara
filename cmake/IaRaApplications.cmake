# IaRa Application Build Functions
# Shared CMake functions for building IaRa applications in tests and experiments

# ==============================================================================
# Environment and Dependency Setup
# ==============================================================================

function(iara_setup_environment)
    # Validate LLVM_INSTALL
    if(NOT DEFINED ENV{LLVM_INSTALL})
        message(FATAL_ERROR "LLVM_INSTALL environment variable not set. Please configure the IaRa environment before building.")
    endif()

    # Set IARA_DIR if not already set
    if(NOT DEFINED ENV{IARA_DIR} OR "$ENV{IARA_DIR}" STREQUAL "")
        set(ENV{IARA_DIR} "${PROJECT_SOURCE_DIR}" PARENT_SCOPE)
    endif()

    # Disable ccache for reproducible builds
    set(CMAKE_C_COMPILER_LAUNCHER "" PARENT_SCOPE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "" PARENT_SCOPE)

    # Find OpenMP library
    set(OpenMP_CXX_FLAGS "-fopenmp" PARENT_SCOPE)
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
    set(OpenMP_CXX_LIBRARY "${OpenMP_CXX_LIBRARY}" PARENT_SCOPE)
    set(OpenMP_CXX_LIBRARY_DIR "${OpenMP_CXX_LIBRARY_DIR}" PARENT_SCOPE)

    # Find bash
    find_program(BASH_EXECUTABLE bash REQUIRED)
    set(BASH_EXECUTABLE "${BASH_EXECUTABLE}" PARENT_SCOPE)

    # Get Clang resource directory
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} --print-resource-dir
        OUTPUT_VARIABLE IARA_CLANG_RESOURCE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT IARA_CLANG_RESOURCE_DIR)
        message(FATAL_ERROR "Failed to query clang resource directory from ${CMAKE_CXX_COMPILER}")
    endif()

    set(IARA_CLANG_INCLUDE_DIR "${IARA_CLANG_RESOURCE_DIR}/include" PARENT_SCOPE)
endfunction()

# ==============================================================================
# Utility Functions
# ==============================================================================

# Collect arguments from environment variable or use defaults
function(iara_collect_args output_var env_var)
    if(DEFINED ENV{${env_var}} AND NOT "$ENV{${env_var}}" STREQUAL "")
        separate_arguments(parsed UNIX_COMMAND "$ENV{${env_var}}")
        set(${output_var} ${parsed} PARENT_SCOPE)
    else()
        set(${output_var} ${ARGN} PARENT_SCOPE)
    endif()
endfunction()

# Determine runtime sources based on scheduler and backend
function(iara_runtime_sources iara_opt_sched runtime_backend out_var out_compile_defs)
    set(srcs "")
    set(compile_defs "")

    if("${iara_opt_sched}" STREQUAL "virtual-fifo")
        # Virtual FIFO runtime sources
        set(srcs
            ${PROJECT_SOURCE_DIR}/runtime/virtual-fifo/Common.cpp
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

# Map scheduler name to IARA components
function(iara_map_scheduler scheduler iara_opt_var runtime_backend_var scheduler_flag_var)
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
        # Sequential baseline scheduler (direct C compilation)
        set(SCHEDULER_FLAG "-DSCHEDULER_SEQUENTIAL")
    elseif("${scheduler}" STREQUAL "vf-sequential")
        # Virtual FIFO without OpenMP - sequential execution of generated code
        set(IARA_OPT_SCHEDULER "virtual-fifo")
        set(IARA_RUNTIME_BACKEND "omp")
        set(SCHEDULER_FLAG "-DSCHEDULER_VF_SEQUENTIAL")
    elseif("${scheduler}" STREQUAL "omp-for")
        set(SCHEDULER_FLAG "-DSCHEDULER_OMP_FOR")
    elseif("${scheduler}" STREQUAL "omp-task")
        set(SCHEDULER_FLAG "-DSCHEDULER_OMP_TASK")
    elseif("${scheduler}" STREQUAL "enkits-task")
        set(SCHEDULER_FLAG "-DSCHEDULER_ENKITS_TASK")
    endif()

    set(${iara_opt_var} ${IARA_OPT_SCHEDULER} PARENT_SCOPE)
    set(${runtime_backend_var} ${IARA_RUNTIME_BACKEND} PARENT_SCOPE)
    set(${scheduler_flag_var} ${SCHEDULER_FLAG} PARENT_SCOPE)
endfunction()

# Check if scheduler is a baseline (non-IaRa) scheduler
function(iara_is_baseline_scheduler scheduler out_var)
    set(is_baseline FALSE)
    if("${scheduler}" STREQUAL "sequential" OR
       "${scheduler}" STREQUAL "omp-task" OR
       "${scheduler}" STREQUAL "omp-for" OR
       "${scheduler}" STREQUAL "enkits-task")
        set(is_baseline TRUE)
    endif()
    set(${out_var} ${is_baseline} PARENT_SCOPE)
endfunction()

# ==============================================================================
# Main Application Build Function
# ==============================================================================

function(iara_add_application)
    # Parse arguments
    cmake_parse_arguments(APP
        ""  # No boolean options
        "ENTRY;SCHEDULER;TARGET_NAME;BUILD_DIR;APP_NAME;TEST_SRC_DIR;APP_SRC_DIR;PARAMETERS_SCRIPT"  # Single-value args
        "EXTRA_KERNEL_ARGS;EXTRA_LINKER_ARGS;CODEGEN_ENV"  # Multi-value args
        ${ARGN}
    )

    # Validate required parameters
    if(NOT APP_SCHEDULER)
        message(FATAL_ERROR "iara_add_application: SCHEDULER is required")
    endif()

    if(NOT APP_TARGET_NAME)
        message(FATAL_ERROR "iara_add_application: TARGET_NAME is required")
    endif()

    if(NOT APP_BUILD_DIR)
        message(FATAL_ERROR "iara_add_application: BUILD_DIR is required")
    endif()

    set(scheduler ${APP_SCHEDULER})
    set(target_name ${APP_TARGET_NAME})
    set(build_subdir ${APP_BUILD_DIR})

    # Auto-detect application sources if not provided
    if(NOT APP_APP_SRC_DIR)
        # Try TEST_SRC_DIR first (set by iara_add_test_instance), then fall back to ENTRY
        if(APP_TEST_SRC_DIR)
            set(APP_APP_SRC_DIR "${APP_TEST_SRC_DIR}")
        elseif(APP_ENTRY)
            # Applications are now numbered (e.g., 05-cholesky, 06-broadcast)
            # Use the entry name directly as the application directory name
            set(APP_APP_SRC_DIR "${PROJECT_SOURCE_DIR}/applications/${APP_ENTRY}")
        else()
            message(FATAL_ERROR "iara_add_application: Either APP_SRC_DIR, TEST_SRC_DIR, or ENTRY must be provided")
        endif()
    endif()

    # Find C/C++ sources (look in src/ subdirectory first, then in app root, then in test dir)
    if(EXISTS "${APP_APP_SRC_DIR}/src")
        file(GLOB c_sources CONFIGURE_DEPENDS "${APP_APP_SRC_DIR}/src/*.c")
        file(GLOB cpp_sources CONFIGURE_DEPENDS "${APP_APP_SRC_DIR}/src/*.cpp")
        set(app_include_dir "${APP_APP_SRC_DIR}/src")
    elseif(EXISTS "${APP_APP_SRC_DIR}")
        file(GLOB c_sources CONFIGURE_DEPENDS "${APP_APP_SRC_DIR}/*.c")
        file(GLOB cpp_sources CONFIGURE_DEPENDS "${APP_APP_SRC_DIR}/*.cpp")
        set(app_include_dir "${APP_APP_SRC_DIR}")
    elseif(APP_TEST_SRC_DIR AND EXISTS "${APP_TEST_SRC_DIR}")
        # Fallback: look for sources directly in test directory (for regression tests)
        file(GLOB c_sources CONFIGURE_DEPENDS "${APP_TEST_SRC_DIR}/*.c")
        file(GLOB cpp_sources CONFIGURE_DEPENDS "${APP_TEST_SRC_DIR}/*.cpp")
        set(app_include_dir "${APP_TEST_SRC_DIR}")
    endif()

    if(NOT c_sources AND NOT cpp_sources)
        message(STATUS "Skipping ${target_name} (no C/C++ sources found in ${APP_APP_SRC_DIR})")
        return()
    endif()

    # Check if baseline scheduler
    # Special case: for regression tests with topology files, sequential is NOT baseline
    # because it needs IaRa code generation, not direct C compilation.
    set(has_topology_file FALSE)
    if(APP_TEST_SRC_DIR)
        if(EXISTS "${APP_TEST_SRC_DIR}/topology.test" OR
           EXISTS "${APP_TEST_SRC_DIR}/topology.mlir" OR
           EXISTS "${APP_TEST_SRC_DIR}/topology.dif")
            set(has_topology_file TRUE)
        endif()
    endif()

    iara_is_baseline_scheduler(${scheduler} is_baseline_scheduler)

    # vf-sequential is always an IaRa scheduler (never baseline)
    if("${scheduler}" STREQUAL "vf-sequential")
        set(is_baseline_scheduler FALSE)
    endif()

    # Get build-type-specific flags
    string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type_upper)
    set(build_type_cxx_flags "${CMAKE_CXX_FLAGS_${build_type_upper}}")
    separate_arguments(build_type_cxx_flags)

    # Map scheduler to IARA components
    iara_map_scheduler(${scheduler} final_iara_opt final_runtime_backend SCHEDULER_FLAG)

    # Override with parameters.sh if available
    if(APP_PARAMETERS_SCRIPT AND EXISTS "${APP_PARAMETERS_SCRIPT}")
        execute_process(
            COMMAND ${BASH_EXECUTABLE} -c "source ${APP_PARAMETERS_SCRIPT} && echo \$IARA_OPT_SCHEDULER"
            OUTPUT_VARIABLE iara_opt_from_params
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
            COMMAND ${BASH_EXECUTABLE} -c "source ${APP_PARAMETERS_SCRIPT} && echo \$IARA_RUNTIME_BACKEND"
            OUTPUT_VARIABLE runtime_backend_from_params
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(iara_opt_from_params)
            set(final_iara_opt ${iara_opt_from_params})
            set(final_runtime_backend ${runtime_backend_from_params})
        endif()
    endif()

    # Get runtime sources
    set(runtime_sources "")
    set(runtime_compile_defs "")
    if(NOT is_baseline_scheduler AND final_iara_opt)
        iara_runtime_sources(${final_iara_opt} ${final_runtime_backend} runtime_sources runtime_compile_defs)
    elseif("${scheduler}" STREQUAL "enkits-task")
        # enkits-task baseline needs EnkiTS
        set(runtime_sources
            ${PROJECT_SOURCE_DIR}/runtime/common/WorkStealingBackend_EnkiTS.cpp
            ${PROJECT_SOURCE_DIR}/external/enkiTS/TaskScheduler.cpp
        )
    endif()

    # Collect extra arguments
    if(NOT is_baseline_scheduler AND final_iara_opt)
        iara_collect_args(iara_flags "IARA_FLAGS" --iara-canonicalize --flatten --${final_iara_opt}=main-actor=run)
    endif()
    iara_collect_args(schedule_extra_args "EXTRA_SCHEDULE_ARGS")
    iara_collect_args(kernel_extra_args "EXTRA_KERNEL_ARGS")
    iara_collect_args(runtime_extra_args "EXTRA_RUNTIME_ARGS")
    iara_collect_args(link_extra_args "EXTRA_LINKER_ARGS")

    # Add user-provided extra args
    if(APP_EXTRA_KERNEL_ARGS)
        list(APPEND kernel_extra_args ${APP_EXTRA_KERNEL_ARGS})
    endif()
    if(APP_EXTRA_LINKER_ARGS)
        list(APPEND link_extra_args ${APP_EXTRA_LINKER_ARGS})
    endif()

    # Handle codegen if script exists and scheduler needs it
    set(codegen_stamp "")
    set(codegen_script "")
    # Only run codegen for IaRa schedulers that need topology and split/join kernels
    if(NOT is_baseline_scheduler AND final_iara_opt)
        if(EXISTS "${APP_APP_SRC_DIR}/codegen.sh")
            set(codegen_script "${APP_APP_SRC_DIR}/codegen.sh")
        elseif(EXISTS "${app_include_dir}/codegen.sh")
            set(codegen_script "${app_include_dir}/codegen.sh")
        elseif(APP_TEST_SRC_DIR AND EXISTS "${APP_TEST_SRC_DIR}/codegen.sh")
            set(codegen_script "${APP_TEST_SRC_DIR}/codegen.sh")
        endif()
    endif()

    # Always add scheduler flag (for both codegen and non-codegen applications)
    if(SCHEDULER_FLAG)
        list(APPEND kernel_extra_args "${SCHEDULER_FLAG}")
    endif()

    if(codegen_script)
        # Get parameter values from environment variables (set by experiment runners)
        set(final_num_blocks "")
        set(final_matrix_size "")
        set(final_image_size "")

        if(DEFINED ENV{NUM_BLOCKS} AND NOT "$ENV{NUM_BLOCKS}" STREQUAL "")
            set(final_num_blocks $ENV{NUM_BLOCKS})
        endif()
        if(DEFINED ENV{MATRIX_SIZE} AND NOT "$ENV{MATRIX_SIZE}" STREQUAL "")
            set(final_matrix_size $ENV{MATRIX_SIZE})
        endif()
        if(DEFINED ENV{IMAGE_SIZE} AND NOT "$ENV{IMAGE_SIZE}" STREQUAL "")
            set(final_image_size $ENV{IMAGE_SIZE})
        endif()

        set(codegen_stamp "${build_subdir}/codegen.stamp")

        # Build environment variables for codegen
        set(codegen_env_cmd
            PATH_TO_APP_SOURCES=${APP_APP_SRC_DIR}
            PATH_TO_TEST_SOURCES=${APP_TEST_SRC_DIR}
            PATH_TO_TEST_BUILD_DIR=${build_subdir}
            NUM_BLOCKS=${final_num_blocks}
            MATRIX_SIZE=${final_matrix_size}
            IMAGE_SIZE=${final_image_size}
            TEST_SCHEDULER=${scheduler}
            IARA_OPT_SCHEDULER=${final_iara_opt}
            IARA_RUNTIME_BACKEND=${final_runtime_backend}
        )
        # Add any additional environment variables from CODEGEN_ENV
        if(APP_CODEGEN_ENV)
            list(APPEND codegen_env_cmd ${APP_CODEGEN_ENV})
        endif()

        add_custom_command(
            OUTPUT ${codegen_stamp}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${build_subdir}
            COMMAND ${CMAKE_COMMAND} -E env ${codegen_env_cmd} sh ${codegen_script}
            COMMAND ${CMAKE_COMMAND} -E touch ${codegen_stamp}
            DEPENDS ${codegen_script}
            WORKING_DIRECTORY ${build_subdir}
            COMMENT "Running codegen for ${target_name}"
            VERBATIM
        )

        # Add compile flags for codegen'd applications
        if(final_num_blocks)
            list(APPEND kernel_extra_args "-DNUM_BLOCKS=${final_num_blocks}")
        endif()
        if(final_matrix_size)
            list(APPEND kernel_extra_args "-DMATRIX_SIZE=${final_matrix_size}")
        endif()
        if(final_image_size)
            list(APPEND kernel_extra_args "-DIMAGE_SIZE=${final_image_size}")
        endif()
    endif()

    # Find iara-opt
    set(iara_opt_binary "$ENV{IARA_DIR}/build/bin/iara-opt")
    if(NOT iara_opt_binary)
        message(FATAL_ERROR "Unable to find iara-opt. Please build iara-opt in the main project.")
    endif()

    # Generate schedule for IaRa schedulers
    set(schedule_obj "")
    if(NOT is_baseline_scheduler)
        # Determine topology input
        set(topology_input "")
        if(DEFINED ENV{TOPOLOGY_FILE} AND NOT "$ENV{TOPOLOGY_FILE}" STREQUAL "")
            set(topology_input "$ENV{TOPOLOGY_FILE}")
        elseif(APP_TEST_SRC_DIR AND EXISTS "${APP_TEST_SRC_DIR}/topology.mlir")
            set(topology_input "${APP_TEST_SRC_DIR}/topology.mlir")
        elseif(APP_TEST_SRC_DIR AND EXISTS "${APP_TEST_SRC_DIR}/topology.dif")
            set(topology_input "${build_subdir}/topology.mlir")
        elseif(APP_TEST_SRC_DIR AND EXISTS "${APP_TEST_SRC_DIR}/topology.test")
            set(topology_input "${APP_TEST_SRC_DIR}/topology.test")
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
        # Always regenerate schedule.mlir when iara-opt changes
        list(APPEND schedule_deps "${iara_opt_binary}")

        # Convert .dif to .mlir if needed
        if(APP_TEST_SRC_DIR AND EXISTS "${APP_TEST_SRC_DIR}/topology.dif")
            add_custom_command(
                OUTPUT ${build_subdir}/topology.mlir
                COMMAND ${CMAKE_COMMAND} -E make_directory ${build_subdir}
                COMMAND python3 ${PROJECT_SOURCE_DIR}/scripts/dif-to-iara.py ${APP_TEST_SRC_DIR}/topology.dif > ${build_subdir}/topology.mlir
                DEPENDS ${APP_TEST_SRC_DIR}/topology.dif ${PROJECT_SOURCE_DIR}/scripts/dif-to-iara.py
                COMMENT "Generating topology.mlir from topology.dif for ${target_name}"
            )
        endif()

        # Generate schedule.mlir
        add_custom_command(
            OUTPUT ${schedule_mlir}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${build_subdir}
            COMMAND ${CMAKE_COMMAND} -E env
                IARA_DIR=$ENV{IARA_DIR}
                PATH_TO_TEST_SOURCES=${APP_TEST_SRC_DIR}
                PATH_TO_TEST_BUILD_DIR=${build_subdir}
                SCHEDULER_MODE=${scheduler}
                ${iara_opt_binary} ${iara_flags} ${topology_input} > ${schedule_mlir}
            DEPENDS ${schedule_deps}
            WORKING_DIRECTORY ${build_subdir}
            COMMENT "Generating schedule.mlir for ${target_name}"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )

        # Lower to LLVM IR
        add_custom_command(
            OUTPUT ${schedule_ll}
            COMMAND ${CMAKE_COMMAND} -E env LLVM_INSTALL=$ENV{LLVM_INSTALL} ${BASH_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/mlir-to-llvmir.sh ${schedule_mlir}
            DEPENDS ${schedule_mlir} ${PROJECT_SOURCE_DIR}/scripts/mlir-to-llvmir.sh
            WORKING_DIRECTORY ${build_subdir}
            COMMENT "Lowering schedule.mlir to LLVM IR for ${target_name}"
            VERBATIM
        )

        # Setup include directories for schedule compilation
        set(schedule_include_args
            -I${IARA_CLANG_INCLUDE_DIR}
            -I${build_subdir}
            -I${PROJECT_SOURCE_DIR}/include
            -I${PROJECT_SOURCE_DIR}/external
        )
        if(APP_TEST_SRC_DIR AND EXISTS "${APP_TEST_SRC_DIR}")
            list(APPEND schedule_include_args -I${APP_TEST_SRC_DIR})
        endif()
        foreach(optional_dir IN ITEMS ${APP_TEST_SRC_DIR}/include ${APP_TEST_SRC_DIR}/generated)
            if(EXISTS "${optional_dir}")
                list(APPEND schedule_include_args -I${optional_dir})
            endif()
        endforeach()

        # Compile schedule.o
        set(schedule_compile_args
            --std=c++20
            ${build_type_cxx_flags}
            ${IARA_BUILD_OPT}
            -stdlib=libstdc++
            ${schedule_extra_args}
            ${schedule_include_args}
            -xir
            -c
            ${schedule_ll}
            -o
            ${schedule_obj}
        )
        # vf-sequential (without OpenMP) doesn't need OpenMP flags
        if(NOT "${scheduler}" STREQUAL "vf-sequential")
            list(APPEND schedule_compile_args ${OpenMP_CXX_FLAGS})
        endif()

        add_custom_command(
            OUTPUT ${schedule_obj}
            COMMAND ${CMAKE_COMMAND} -E env LLVM_INSTALL=$ENV{LLVM_INSTALL} ${CMAKE_CXX_COMPILER} ${schedule_compile_args}
            DEPENDS ${schedule_ll}
            WORKING_DIRECTORY ${build_subdir}
            COMMENT "Compiling schedule.ll for ${target_name}"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()

    # Build executable
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

    # Setup include directories
    set(test_include_dirs
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external
        ${PROJECT_SOURCE_DIR}/external/enkiTS/src
        ${app_include_dir}
        ${build_subdir}
    )
    if(APP_TEST_SRC_DIR)
        list(APPEND test_include_dirs ${APP_TEST_SRC_DIR})
        foreach(extra_dir IN ITEMS ${APP_TEST_SRC_DIR}/include ${APP_TEST_SRC_DIR}/src ${APP_TEST_SRC_DIR}/generated)
            if(EXISTS "${extra_dir}")
                list(APPEND test_include_dirs "${extra_dir}")
            endif()
        endforeach()
    endif()

    # Find casacore if available (for degridder application)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(CASACORE casacore QUIET)
        if(CASACORE_FOUND)
            list(APPEND test_include_dirs ${CASACORE_INCLUDE_DIRS})
        endif()
    endif()

    target_include_directories(${target_name} PRIVATE ${test_include_dirs})

    # Compile options
    # vf-sequential (without OpenMP) doesn't need OpenMP flags
    set(common_compile_options "")
    if(NOT "${scheduler}" STREQUAL "vf-sequential")
        list(APPEND common_compile_options ${OpenMP_CXX_FLAGS})
    endif()
    list(APPEND common_compile_options ${build_type_cxx_flags})
    list(APPEND common_compile_options ${IARA_BUILD_OPT})
    list(APPEND common_compile_options ${kernel_extra_args})
    list(APPEND common_compile_options ${runtime_extra_args})
    if(runtime_compile_defs)
        list(APPEND common_compile_options "-D${runtime_compile_defs}")
    endif()

    target_compile_options(${target_name}
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libstdc++>
        ${common_compile_options}
        -UIARA_COMPILER
    )

    # Link options
    target_link_directories(${target_name} PRIVATE $ENV{LLVM_INSTALL}/lib)
    if(OpenMP_CXX_LIBRARY_DIR)
        target_link_directories(${target_name} PRIVATE ${OpenMP_CXX_LIBRARY_DIR})
    endif()
    # Add casacore library directories if found
    if(CASACORE_FOUND)
        target_link_directories(${target_name} PRIVATE ${CASACORE_LIBRARY_DIRS})
    else()
        # If pkg-config didn't find casacore, try adding standard locations from CMAKE_PREFIX_PATH
        foreach(prefix ${CMAKE_PREFIX_PATH})
            if(EXISTS "${prefix}/lib")
                target_link_directories(${target_name} PRIVATE "${prefix}/lib")
            endif()
            if(EXISTS "${prefix}/lib64")
                target_link_directories(${target_name} PRIVATE "${prefix}/lib64")
            endif()
        endforeach()
    endif()
    # vf-sequential doesn't need OpenMP library
    set(link_libraries "m" "pthread")
    if(NOT "${scheduler}" STREQUAL "vf-sequential")
        list(APPEND link_libraries ${OpenMP_CXX_LIBRARY})
    endif()
    target_link_libraries(${target_name} PRIVATE ${link_libraries})

    set(link_options "-stdlib=libstdc++" "-Wl,--gc-sections")
    if(NOT "${scheduler}" STREQUAL "vf-sequential")
        list(APPEND link_options ${OpenMP_CXX_FLAGS})
    endif()
    target_link_options(${target_name} PRIVATE ${link_options})
    if(OpenMP_CXX_LIBRARY_DIR)
        target_link_options(${target_name} PRIVATE -Wl,-rpath,${OpenMP_CXX_LIBRARY_DIR})
    else()
        if(DEFINED ENV{LLVM_INSTALL} AND NOT "$ENV{LLVM_INSTALL}" STREQUAL "")
            target_link_options(${target_name} PRIVATE -Wl,-rpath,$ENV{LLVM_INSTALL}/lib -Wl,-rpath,$ENV{LLVM_INSTALL}/lib/x86_64-unknown-linux-gnu)
        endif()
    endif()
    if(link_extra_args)
        target_link_options(${target_name} PRIVATE ${link_extra_args})
    endif()
endfunction()

# ==============================================================================
# Test Instance Wrapper Function
# ==============================================================================

function(iara_add_test_instance)
    # Parse arguments
    cmake_parse_arguments(TEST
        ""  # No boolean options
        "NAME;EXPERIMENT_SET;APPLICATION_DIR;ENTRY;SCHEDULER;BUILD_DIR"  # Single-value args
        "PARAMETERS;DEFINES;LINKER_ARGS"  # Multi-value args
        ${ARGN}
    )

    # Validate required parameters
    if(NOT TEST_NAME)
        message(FATAL_ERROR "iara_add_test_instance: NAME is required")
    endif()
    if(NOT TEST_ENTRY)
        message(FATAL_ERROR "iara_add_test_instance: ENTRY is required")
    endif()
    if(NOT TEST_SCHEDULER)
        message(FATAL_ERROR "iara_add_test_instance: SCHEDULER is required")
    endif()
    if(NOT TEST_BUILD_DIR)
        message(FATAL_ERROR "iara_add_test_instance: BUILD_DIR is required")
    endif()

    set(instance_name "${TEST_NAME}")
    set(target_name "run-${instance_name}")
    set(build_target_name "build-${instance_name}")

    # Set environment variables for compilation (for parameter defines)
    foreach(param ${TEST_PARAMETERS})
        # param is in format "name=value"
        string(REGEX MATCH "^([^=]+)=(.*)$" _ "${param}")
        if(CMAKE_MATCH_1)
            set(ENV{${CMAKE_MATCH_1}} "${CMAKE_MATCH_2}")
        endif()
    endforeach()

    # Set environment variables for defines and collect as compiler flags
    set(extra_kernel_args_list "")
    set(codegen_env_list "")
    foreach(define ${TEST_DEFINES})
        # define is in format "NAME=value" or "NAME"
        string(REGEX MATCH "^([^=]+)=?(.*)$" _ "${define}")
        if(CMAKE_MATCH_1)
            set(define_name "${CMAKE_MATCH_1}")
            set(define_value "${CMAKE_MATCH_2}")
            if(define_value)
                set(ENV{${define_name}} "${define_value}")
                list(APPEND extra_kernel_args_list "-D${define_name}=${define_value}")
                # Also add to codegen environment
                list(APPEND codegen_env_list "${define_name}=${define_value}")
            else()
                list(APPEND extra_kernel_args_list "-D${define_name}")
            endif()
        endif()
    endforeach()

    # Convert linker args to list
    set(linker_args_list "")
    if(TEST_LINKER_ARGS)
        string(REPLACE " " ";" linker_args_list "${TEST_LINKER_ARGS}")
    endif()

    # Call iara_add_application to do the actual compilation
    iara_add_application(
        ENTRY "${TEST_ENTRY}"
        SCHEDULER "${TEST_SCHEDULER}"
        TARGET_NAME "${target_name}"
        BUILD_DIR "${TEST_BUILD_DIR}"
        TEST_SRC_DIR "${CMAKE_SOURCE_DIR}/${TEST_APPLICATION_DIR}"
        EXTRA_KERNEL_ARGS ${extra_kernel_args_list}
        EXTRA_LINKER_ARGS ${linker_args_list}
        CODEGEN_ENV ${codegen_env_list}
    )

    # Create build-* custom target that forces a full rebuild
    # This allows timing the entire compilation (not just linking) during the test
    # We clean build artifacts to force recompilation of all source files
    # CRITICAL: To prevent CMake from caching this target, we make it depend on a PHONY target
    # This forces CMake to ALWAYS rebuild because the phony target never produces output

    # Create phony target that never completes (forces parent to always rebuild)
    add_custom_target(${build_target_name}_phony
        COMMAND ${CMAKE_COMMAND} -E echo "Building ${build_target_name}"
    )

    # Main build target depends on phony target, so it ALWAYS rebuilds
    # The phony dependency ensures CMake never caches this target as "already built"
    add_custom_target(${build_target_name}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${TEST_BUILD_DIR}"
        # Build the target (CMake will recompile because of phony dependency)
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${target_name}
        DEPENDS ${build_target_name}_phony
        VERBATIM
    )

    # Register build-* as a CTest test
    add_test(
        NAME "${build_target_name}"
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${build_target_name}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    # Set build test properties
    set_tests_properties("${build_target_name}" PROPERTIES
        FIXTURES_SETUP "${build_target_name}"
        TIMEOUT 600
    )

    # Register run-* as a CTest test that depends on build-*
    add_test(
        NAME "${target_name}"
        COMMAND ${target_name}
        WORKING_DIRECTORY "${TEST_BUILD_DIR}"
    )

    # Set run test properties (depends on build test via fixtures)
    set_tests_properties("${target_name}" PROPERTIES
        FIXTURES_REQUIRED "${build_target_name}"
    )

endfunction()
