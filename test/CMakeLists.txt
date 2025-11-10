include(CTest)

# Scheduler modes to test
set(SCHEDULER_MODES "virtual-fifo")
# set(SCHEDULER_MODES "virtual-fifo" "ring-buffer")

file(GLOB entries RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/Iara" "${CMAKE_CURRENT_SOURCE_DIR}/Iara/*")
foreach(entry IN LISTS entries)
        if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Iara/${entry}")
                # Add a build target and test for each scheduler mode
                foreach(scheduler IN LISTS SCHEDULER_MODES)
                        set(test_name "${entry}-${scheduler}")
                        set(build_target "build-${test_name}")
                        
                        message(STATUS "Adding build target: ${build_target}")
                        
                        # Create a custom target that builds this specific test
                        add_custom_target(${build_target}
                                COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/build-single-test.sh 
                                        ${CMAKE_CURRENT_SOURCE_DIR}/Iara/${entry} 
                                        ${scheduler}
                                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                                COMMENT "Building test ${test_name}"
                                VERBATIM
                        )
                        
                        # Make the build target depend on iara-opt
                        add_dependencies(${build_target} iara-opt)
                        
                        message(STATUS "Adding test: ${test_name}")
                        add_test(
                                NAME ${test_name}
                                COMMAND run-single-test.sh ${entry} ${scheduler}
                                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        )
                        
                        # Tests should depend on their build targets
                        set_tests_properties(${test_name} PROPERTIES
                                FIXTURES_REQUIRED ${build_target}
                        )
                        
                        # Add a fixture setup that runs the build
                        add_test(
                                NAME ${build_target}
                                COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${build_target}
                        )
                        set_tests_properties(${build_target} PROPERTIES
                                FIXTURES_SETUP ${build_target}
                        )
                endforeach()
        endif()
endforeach()
