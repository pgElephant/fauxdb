# Regression Tests for FauxDB
cmake_minimum_required(VERSION 3.16)

# Find required packages
find_package(GTest REQUIRED)
find_package(PkgConfig REQUIRED)

# Find libmongoc (mongo-c-driver) for integration tests
pkg_check_modules(MONGOC mongoc2)

# Find nlohmann/json for result comparison
find_package(nlohmann_json REQUIRED)

# Test source files (if any unit tests exist)
set(UNIT_TEST_SOURCES
    # Add unit test files here when they exist
    # unit/test_types.cpp
)

# Integration test sources
if(MONGOC_FOUND)
    set(INTEGRATION_TEST_SOURCES
        integration/client_regression.cpp
    )
    
    # Create integration test executable
    add_executable(client_regression_tests ${INTEGRATION_TEST_SOURCES})
    
    # Add test sources to executable
    target_sources(client_regression_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src/types/CTypes.cpp
        ${CMAKE_SOURCE_DIR}/src/CConfiguration.cpp
        ${CMAKE_SOURCE_DIR}/src/CLogger.cpp
    )
    
    # Link libraries for integration tests
    target_link_libraries(client_regression_tests
        GTest::gtest
        GTest::gtest_main
        ${MONGOC_LIBRARIES}
        nlohmann_json::nlohmann_json
    )
    
    # Include directories for integration tests
    target_include_directories(client_regression_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${MONGOC_INCLUDE_DIRS}
    )
    include_directories(
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/protocol
        ${CMAKE_SOURCE_DIR}/include/database
        ${CMAKE_SOURCE_DIR}/include/network
        ${CMAKE_SOURCE_DIR}/include/parsing
        ${CMAKE_SOURCE_DIR}/include/query_response
        ${CMAKE_SOURCE_DIR}/include/types
    )
    
    # Compiler definitions for integration tests
    target_compile_definitions(client_regression_tests PRIVATE ${MONGOC_CFLAGS_OTHER})
    
    # Set library directories for integration tests
    target_link_directories(client_regression_tests PRIVATE 
        ${MONGOC_LIBRARY_DIRS}
    )
    
    # Add integration tests
    add_test(NAME ClientRegressionTests COMMAND client_regression_tests)
    
    # Set test properties
    set_tests_properties(ClientRegressionTests PROPERTIES TIMEOUT 120)
    
    message(STATUS "Document client integration tests enabled")
else()
    message(STATUS "Document client integration tests disabled (missing dependencies)")
    if(NOT MONGOC_FOUND)
        message(STATUS "  - libmongoc (mongo-c-driver) not found")
    endif()
endif()

# Create unit test executable only if sources exist
if(UNIT_TEST_SOURCES)
    add_executable(fauxdb_unit_tests ${UNIT_TEST_SOURCES})
    
    target_sources(fauxdb_unit_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src/types/CTypes.cpp
        ${CMAKE_SOURCE_DIR}/src/CConfiguration.cpp
        ${CMAKE_SOURCE_DIR}/src/CLogger.cpp
    )
    
    target_link_libraries(fauxdb_unit_tests
        GTest::gtest
        GTest::gtest_main
    )
    
    target_include_directories(fauxdb_unit_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    
    add_test(NAME UnitTests COMMAND fauxdb_unit_tests)
    set_tests_properties(UnitTests PROPERTIES TIMEOUT 30)
endif()

# Custom targets for running tests
add_custom_target(regression
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all regression tests"
)

# Custom target for running JavaScript-based tests
add_custom_target(js_regression
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_regression_tests.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running JavaScript-based regression tests"
)

# Custom target for quick document database shell test
add_custom_target(quick_test
    COMMAND export CLIENT=mongosh && $$CLIENT --host 127.0.0.1 --port 27018 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/regression_tests.js
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running quick document database shell regression test"
)
