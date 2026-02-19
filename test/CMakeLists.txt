include(CTest)
include(${PROJECT_SOURCE_DIR}/cmake/IaRaApplications.cmake)

# Setup IaRa build environment
iara_setup_environment()

# Ensure the main iara build is up-to-date with source changes
# This is necessary so that iara-opt includes any recent changes to the IaRa dialect
add_custom_target(rebuild-iara-opt
        COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR} --target iara-opt
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
        COMMENT "Rebuilding iara-opt from main build directory"
)

# Scheduler modes to test; only set defaults if the variable isn't already set
if(NOT DEFINED SCHEDULER_MODES OR "${SCHEDULER_MODES}" STREQUAL "")
        set(SCHEDULER_MODES "virtual-fifo")
endif()
# set(SCHEDULER_MODES "virtual-fifo" "ring-buffer")

function(iara_add_runtime_test entry scheduler)
        set(test_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/Iara/${entry}")
        if(NOT IS_DIRECTORY "${test_src_dir}")
                return()
        endif()

        # Check if entry already ends with the scheduler suffix to avoid duplication
        # e.g., entry="03-two-nodes_vf-sequential", scheduler="vf-sequential" -> base="03-two-nodes_vf-sequential"
        set(base "${entry}")
        if(NOT "${entry}" MATCHES "_${scheduler}$")
                # entry doesn't already have the scheduler suffix, so append it
                set(base "${entry}-${scheduler}")
        endif()
        message(STATUS "Configuring IaRa test ${base}")

        set(test_binary_root "${CMAKE_CURRENT_BINARY_DIR}/Iara/${entry}")
        set(build_subdir "${test_binary_root}/build-${scheduler}")

        # Set up parameters script path
        set(parameters_script "")
        if(EXISTS "${test_src_dir}/parameters.sh")
                set(parameters_script "${test_src_dir}/parameters.sh")
        endif()

        # Load test instance CMakeLists.txt to set EXTRA_KERNEL_ARGS and EXTRA_LINKER_ARGS
        # from experiments.yaml configuration via generate_cmakelists.py
        set(cmake_file "${test_src_dir}/CMakeLists.txt")
        if(EXISTS "${cmake_file}")
                # Include the file to populate cache variables
                include("${cmake_file}")
                # Retrieve values from cache (used by iara_add_application)
                get_property(EXTRA_KERNEL_ARGS CACHE EXTRA_KERNEL_ARGS PROPERTY VALUE)
                get_property(EXTRA_LINKER_ARGS CACHE EXTRA_LINKER_ARGS PROPERTY VALUE)
        endif()

        # Call unified build function
        set(target_name "run-${base}")

        # Check if this test has sources using the same logic as iara_add_application
        # This matches the precedence: app/src/ > app/ > test/
        # Applications are now numbered (e.g., 03-two-nodes), so we look for them directly
        set(app_src_dir "${PROJECT_SOURCE_DIR}/applications/${entry}")

        set(has_sources FALSE)

        # Priority 1: application src/ subdirectory
        if(EXISTS "${app_src_dir}/src")
            file(GLOB app_src_files "${app_src_dir}/src/*.c" "${app_src_dir}/src/*.cpp")
            if(app_src_files)
                set(has_sources TRUE)
            endif()
        endif()

        # Priority 2: application root directory
        if(NOT has_sources AND EXISTS "${app_src_dir}")
            file(GLOB app_src_files "${app_src_dir}/*.c" "${app_src_dir}/*.cpp")
            if(app_src_files)
                set(has_sources TRUE)
            endif()
        endif()

        # Priority 3: test directory (only if no app sources found)
        if(NOT has_sources)
            file(GLOB test_src_files "${test_src_dir}/*.c" "${test_src_dir}/*.cpp")
            if(test_src_files)
                set(has_sources TRUE)
            endif()
        endif()

        if(NOT has_sources)
            # No sources found, skip this test configuration
            return()
        endif()

        iara_add_application(
                ENTRY ${entry}
                SCHEDULER ${scheduler}
                TARGET_NAME ${target_name}
                BUILD_DIR ${build_subdir}
                TEST_SRC_DIR ${test_src_dir}
                PARAMETERS_SCRIPT ${parameters_script}
                EXTRA_KERNEL_ARGS ${EXTRA_KERNEL_ARGS}
                EXTRA_LINKER_ARGS ${EXTRA_LINKER_ARGS}
        )

        # Create build target and CTest definitions
        # Depend on rebuild-iara-opt to ensure iara-opt is up-to-date with source changes
        add_custom_target(build-${base} DEPENDS ${target_name} rebuild-iara-opt)

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
                # Filter tests if IARA_TEST_FILTER is set
                if(DEFINED ENV{IARA_TEST_FILTER} AND NOT "$ENV{IARA_TEST_FILTER}" STREQUAL "")
                        string(FIND "${entry}" "$ENV{IARA_TEST_FILTER}" filter_pos)
                        if(${filter_pos} EQUAL -1)
                                # Test name doesn't contain filter substring, skip it
                                continue()
                        endif()
                endif()

                # Check if test directory name encodes a specific scheduler
                # e.g., "05.1-cholesky-10080_10_vf-omp" -> should only build with vf-omp
                # Extract scheduler from directory suffix
                set(specific_scheduler "")
                if("${entry}" MATCHES "_vf-omp$")
                        set(specific_scheduler "vf-omp")
                elseif("${entry}" MATCHES "_vf-enkits$")
                        set(specific_scheduler "vf-enkits")
                elseif("${entry}" MATCHES "_vf-sequential$")
                        set(specific_scheduler "vf-sequential")
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
                        # Note: entry directory already contains the scheduler suffix (e.g., "03-two-nodes_vf-sequential")
                        # but we pass just the scheduler to iara_add_runtime_test, which will build with
                        # the test_binary_root using the entry directory name and create test names without duplication
                        iara_add_runtime_test(${entry} ${specific_scheduler})
                else()
                        # No specific scheduler in name - build with all schedulers in SCHEDULER_MODES
                        foreach(scheduler IN LISTS SCHEDULER_MODES)
                                iara_add_runtime_test(${entry} ${scheduler})
                        endforeach()
                endif()
        endif()
endforeach()
