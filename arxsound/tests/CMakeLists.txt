# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# ============================================================================
# Core library tests
# ============================================================================
add_executable(test_arxsound_core
    test_arxsound_core.cpp
)

target_link_libraries(test_arxsound_core PRIVATE
    ArxSound
    pthread
    m
)

target_include_directories(test_arxsound_core PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(test_arxsound_core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME ArxSoundCoreTests COMMAND test_arxsound_core)

# ============================================================================
# Integration tests (ArxGlue)
# ============================================================================
add_executable(test_arxsound_arxglue_integration
    test_arxsound_arxglue_integration.cpp
)

target_link_libraries(test_arxsound_arxglue_integration PRIVATE
    ArxSound
    ArxGlue
    pthread
    m
)

target_include_directories(test_arxsound_arxglue_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/arxglue
)

set_target_properties(test_arxsound_arxglue_integration PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME ArxSoundArxGlueIntegrationTests COMMAND test_arxsound_arxglue_integration)

# ============================================================================
# Custom target for running all tests
# ============================================================================
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_arxsound_core test_arxsound_arxglue_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all ArxSound tests"
)