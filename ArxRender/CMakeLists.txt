cmake_minimum_required(VERSION 3.16)
project(ArxRender VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ARXRENDER_BUILD_SHARED "Build shared library" ON)
option(ARXRENDER_ENABLE_VULKAN "Enable Vulkan" OFF)

include(FetchContent)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.3.8)
FetchContent_MakeAvailable(glfw)

# GLAD (или используйте локальный glad.c/h)
FetchContent_Declare(glad GIT_REPOSITORY https://github.com/Dav1dde/glad.git GIT_TAG v0.1.36)
set(GLAD_PROFILE "core" CACHE STRING "" FORCE)
set(GLAD_API "gl=4.5" CACHE STRING "" FORCE)
set(GLAD_GENERATOR "c" CACHE STRING "" FORCE)
FetchContent_MakeAvailable(glad)

if(ARXRENDER_BUILD_SHARED)
    add_library(ArxRender SHARED)
else()
    add_library(ArxRender STATIC)
endif()

target_compile_definitions(ArxRender PRIVATE ARXRENDER_EXPORTS)
target_include_directories(ArxRender PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_sources(ArxRender PRIVATE
    src/arxrender.cpp
    src/arxrender_context.cpp
    src/arxrender_device.cpp
    src/arxrender_log.cpp
    src/arxrender_resource.cpp
    src/arxrender_pipeline.cpp
    src/arxrender_command.cpp
    src/arxrender_surface.cpp
    src/backends/backend_null.cpp
    src/backends/backend_opengl_es.cpp
)

target_link_libraries(ArxRender PRIVATE glfw glad)

if(MSVC)
    target_compile_options(ArxRender PRIVATE /W4 /EHsc /MP)
    target_compile_definitions(ArxRender PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    target_compile_options(ArxRender PRIVATE -Wall -Wextra -Wpedantic)
endif()