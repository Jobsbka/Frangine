cmake_minimum_required(VERSION 3.16)
project(3tone VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Загрузка GLFW через FetchContent (требуется для рендера)
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
FetchContent_MakeAvailable(glfw)

# Исполняемый файл со всеми исходниками
add_executable(3tone
    src/main.cpp
    src/glad.c
    src/core/graph.cpp
    src/nodes/text_texture_node.cpp
    src/core/executor.cpp
    src/core/baking.cpp
    src/assets/asset_manager.cpp
    src/assets/stb_impl.cpp
    src/nodes/basic_nodes.cpp
    src/nodes/asset_node.cpp
    src/nodes/checker_texture.cpp
    src/nodes/convert_node.cpp
    src/nodes/render_nodes.cpp
    src/render/graphics_device.cpp
    src/render/texture.cpp
    src/render/mesh.cpp
    src/render/shader.cpp
    src/render/material.cpp
    src/render/camera.cpp
    src/render/scene.cpp
    src/render/command_buffer.cpp
    src/render/render_target.cpp
    src/game/Game.cpp
    src/ui/input_manager.cpp
    src/ui/ui_nodes.cpp
    src/ui/ui_render_node.cpp
)

# Директории с заголовочными файлами
target_include_directories(3tone PRIVATE src)
target_include_directories(3tone PRIVATE include)   # nlohmann/json, glad, KHR

# Опции компиляции
if(MSVC)
    target_compile_options(3tone PRIVATE /W4 /EHsc)
    target_compile_definitions(3tone PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(3tone PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Линковка с потоками и GLFW (OpenGL подтягивается через GLFW)
find_package(Threads REQUIRED)
target_link_libraries(3tone Threads::Threads glfw)

# Для Windows: автоматическое копирование DLL GLFW (если используется динамическая сборка)
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET 3tone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:glfw>
        $<TARGET_FILE_DIR:3tone>
    )
endif()