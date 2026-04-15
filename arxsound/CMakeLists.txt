cmake_minimum_required(VERSION 3.16)
project(ArxSound VERSION 0.1.0 LANGUAGES CXX)

# ============================================================================
# Project Settings
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build options
option(AS_BUILD_SHARED "Build as shared library" OFF)
option(AS_BUILD_TESTS "Build test executable" ON)
option(AS_BUILD_EXAMPLES "Build examples" ON)
option(AS_ENABLE_WASAPI "Enable WASAPI backend (Windows)" ON)
option(AS_ENABLE_DSOUND "Enable DirectSound backend (Windows)" ON)
option(AS_ENABLE_WINMM "Enable WinMM backend (Windows)" ON)
option(AS_ENABLE_ALSA "Enable ALSA backend (Linux)" ON)
option(AS_ENABLE_PULSEAUDIO "Enable PulseAudio backend (Linux)" ON)
option(AS_ENABLE_JACK "Enable JACK backend (Linux)" ON)
option(AS_ENABLE_COREAUDIO "Enable CoreAudio backend (macOS/iOS)" ON)
option(AS_ENABLE_AAUDIO "Enable AAudio backend (Android)" ON)
option(AS_ENABLE_OPENSL "Enable OpenSL backend (Android)" ON)
option(AS_ENABLE_WEBAUDIO "Enable WebAudio backend (Emscripten)" ON)
option(AS_ENABLE_ARXGLUE "Enable ArxGlue integration" ON)

# ============================================================================
# Source Files
# ============================================================================
set(AS_CORE_SOURCES
    src/arxsound.cpp
    src/arxsound_log.cpp
    src/arxsound_context.cpp
    src/arxsound_device.cpp
    src/arxsound_convert.cpp
    src/backends/backend_null.cpp
)

set(AS_HEADERS
    include/arxsound.hpp
    include/arxsound_types.hpp
    include/arxsound_log.hpp
    include/arxsound_context.hpp
    include/arxsound_device.hpp
    include/arxsound_convert.hpp
    include/arxsound_backend.hpp
)

# ============================================================================
# Platform-Specific Sources
# ============================================================================
if(WIN32)
    if(AS_ENABLE_WASAPI)
        list(APPEND AS_CORE_SOURCES src/backends/backend_wasapi.cpp)
        add_compile_definitions(AS_HAS_WASAPI)
    endif()
    if(AS_ENABLE_DSOUND)
        list(APPEND AS_CORE_SOURCES src/backends/backend_dsound.cpp)
        add_compile_definitions(AS_HAS_DSOUND)
    endif()
    if(AS_ENABLE_WINMM)
        list(APPEND AS_CORE_SOURCES src/backends/backend_winmm.cpp)
        add_compile_definitions(AS_HAS_WINMM)
    endif()
elseif(LINUX)
    if(AS_ENABLE_ALSA)
        list(APPEND AS_CORE_SOURCES src/backends/backend_alsa.cpp)
        add_compile_definitions(AS_HAS_ALSA)
        find_package(ALSA)
    endif()
    if(AS_ENABLE_PULSEAUDIO)
        list(APPEND AS_CORE_SOURCES src/backends/backend_pulseaudio.cpp)
        add_compile_definitions(AS_HAS_PULSEAUDIO)
        find_package(PulseAudio)
    endif()
    if(AS_ENABLE_JACK)
        list(APPEND AS_CORE_SOURCES src/backends/backend_jack.cpp)
        add_compile_definitions(AS_HAS_JACK)
        find_package(JACK)
    endif()
elseif(APPLE)
    if(AS_ENABLE_COREAUDIO)
        list(APPEND AS_CORE_SOURCES src/backends/backend_coreaudio.cpp)
        add_compile_definitions(AS_HAS_COREAUDIO)
    endif()
elseif(ANDROID)
    if(AS_ENABLE_AAUDIO)
        list(APPEND AS_CORE_SOURCES src/backends/backend_aaudio.cpp)
        add_compile_definitions(AS_HAS_AAUDIO)
    endif()
    if(AS_ENABLE_OPENSL)
        list(APPEND AS_CORE_SOURCES src/backends/backend_opensl.cpp)
        add_compile_definitions(AS_HAS_OPENSL)
    endif()
elseif(EMSCRIPTEN)
    if(AS_ENABLE_WEBAUDIO)
        list(APPEND AS_CORE_SOURCES src/backends/backend_webaudio.cpp)
        add_compile_definitions(AS_HAS_WEBAUDIO)
    endif()
endif()

# ArxGlue integration
if(AS_ENABLE_ARXGLUE)
    list(APPEND AS_CORE_SOURCES arxglue/arxsound_node.cpp)
    add_compile_definitions(AS_HAS_ARXGLUE)
endif()

# ============================================================================
# Library Target
# ============================================================================
if(AS_BUILD_SHARED)
    add_library(ArxSound SHARED ${AS_CORE_SOURCES} ${AS_HEADERS})
    target_compile_definitions(ArxSound PUBLIC AS_BUILD_SHARED)
else()
    add_library(ArxSound STATIC ${AS_CORE_SOURCES} ${AS_HEADERS})
endif()

target_include_directories(ArxSound
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/arxglue
)

# ============================================================================
# Platform Libraries
# ============================================================================
if(WIN32)
    target_link_libraries(ArxSound PRIVATE
        ole32
        mmdevapi
        user32
    )
    if(AS_ENABLE_DSOUND)
        target_link_libraries(ArxSound PRIVATE dsound)
    endif()
    if(AS_ENABLE_WINMM)
        target_link_libraries(ArxSound PRIVATE winmm)
    endif()
elseif(LINUX)
    target_link_libraries(ArxSound PRIVATE
        pthread
        dl
        m
    )
    if(AS_ENABLE_ALSA AND ALSA_FOUND)
        target_link_libraries(ArxSound PRIVATE ${ALSA_LIBRARIES})
        target_include_directories(ArxSound PRIVATE ${ALSA_INCLUDE_DIRS})
    endif()
    if(AS_ENABLE_PULSEAUDIO AND PulseAudio_FOUND)
        target_link_libraries(ArxSound PRIVATE ${PULSEAUDIO_LIBRARIES})
    endif()
    if(AS_ENABLE_JACK AND JACK_FOUND)
        target_link_libraries(ArxSound PRIVATE ${JACK_LIBRARIES})
    endif()
elseif(APPLE)
    target_link_libraries(ArxSound PRIVATE
        "-framework CoreAudio"
        "-framework AudioUnit"
        "-framework AudioToolbox"
        "-framework CoreFoundation"
    )
elseif(ANDROID)
    target_link_libraries(ArxSound PRIVATE
        android
        log
    )
    if(AS_ENABLE_OPENSL)
        target_link_libraries(ArxSound PRIVATE OpenSLES)
    endif()
elseif(EMSCRIPTEN)
    target_link_options(ArxSound PRIVATE
        -s USE_WEBAUDIO=1
        -s ALLOW_MEMORY_GROWTH=1
        -s ASYNCIFY
    )
endif()

# ============================================================================
# Test Executable
# ============================================================================
if(AS_BUILD_TESTS)
    add_executable(ArxSoundTest
        tests/main.cpp
    )
    
    target_link_libraries(ArxSoundTest PRIVATE
        ArxSound
    )
    
    target_include_directories(ArxSoundTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/arxglue
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
    
    set_target_properties(ArxSoundTest PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        OUTPUT_NAME "arxsound_test"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    
    # Copy to build root for easy access
    add_custom_command(TARGET ArxSoundTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:ArxSoundTest>
        ${CMAKE_BINARY_DIR}/arxsound_test$<EXE_SUFFIX>
        COMMENT "Copying test executable to build root"
    )
endif()

# ============================================================================
# Examples
# ============================================================================
if(AS_BUILD_EXAMPLES)
    # Simple playback example
    add_executable(ExamplePlayback
        examples/playback.cpp
    )
    target_link_libraries(ExamplePlayback PRIVATE ArxSound)
    target_include_directories(ExamplePlayback PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    set_target_properties(ExamplePlayback PROPERTIES
        OUTPUT_NAME "example_playback"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    
    # ArxGlue graph example
    if(AS_ENABLE_ARXGLUE)
        add_executable(ExampleGraph
            examples/graph_demo.cpp
        )
        target_link_libraries(ExampleGraph PRIVATE ArxSound)
        target_include_directories(ExampleGraph PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/arxglue
        )
        set_target_properties(ExampleGraph PROPERTIES
            OUTPUT_NAME "example_graph"
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        )
    endif()
endif()

# ============================================================================
# Installation
# ============================================================================
include(GNUInstallDirs)

install(TARGETS ArxSound
    EXPORT ArxSoundTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT ArxSoundTargets
    FILE ArxSoundTargets.cmake
    NAMESPACE ArxSound::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ArxSound
)

# ============================================================================
# Package Config
# ============================================================================
include(CMakePackageConfigHelpers)

# Create cmake directory if it doesn't exist
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArxSoundConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ArxSoundConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ArxSound
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ArxSoundConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ArxSoundConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ArxSoundConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ArxSound
)

# ============================================================================
# Print Configuration
# ============================================================================
message(STATUS "========================================")
message(STATUS "ArxSound Configuration")
message(STATUS "========================================")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build Shared: ${AS_BUILD_SHARED}")
message(STATUS "Build Tests: ${AS_BUILD_TESTS}")
message(STATUS "Build Examples: ${AS_BUILD_EXAMPLES}")
message(STATUS "ArxGlue Integration: ${AS_ENABLE_ARXGLUE}")
message(STATUS "----------------------------------------")
message(STATUS "Backends:")
message(STATUS "  WASAPI: ${AS_ENABLE_WASAPI}")
message(STATUS "  DirectSound: ${AS_ENABLE_DSOUND}")
message(STATUS "  WinMM: ${AS_ENABLE_WINMM}")
message(STATUS "  ALSA: ${AS_ENABLE_ALSA}")
message(STATUS "  PulseAudio: ${AS_ENABLE_PULSEAUDIO}")
message(STATUS "  JACK: ${AS_ENABLE_JACK}")
message(STATUS "  CoreAudio: ${AS_ENABLE_COREAUDIO}")
message(STATUS "  AAudio: ${AS_ENABLE_AAUDIO}")
message(STATUS "  OpenSL: ${AS_ENABLE_OPENSL}")
message(STATUS "  WebAudio: ${AS_ENABLE_WEBAUDIO}")
message(STATUS "========================================")