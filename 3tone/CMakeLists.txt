cmake_minimum_required(VERSION 3.16)
project(3tone VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

include(FetchContent)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.3.8)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3)
FetchContent_MakeAvailable(json)

find_package(Threads REQUIRED)

add_executable(3tone
    src/glad.c
    src/main.cpp
    # core
    src/core/graph.cpp
    src/core/executor.cpp
    src/core/baking.cpp
    src/core/core_module.cpp
    # render
    src/render/graphics_device.cpp
    src/render/texture.cpp
    src/render/mesh.cpp
    src/render/shader.cpp
    src/render/material.cpp
    src/render/camera.cpp
    src/render/scene.cpp
    src/render/command_buffer.cpp
    src/render/render_target.cpp
    src/render/render_module.cpp
    src/render/font.cpp
    # sound
    src/sound/miniaudio_backend.cpp
    src/sound/audio_graph.cpp
    src/sound/ga_spatial.cpp
    src/sound/dsp/biquad.cpp
    src/sound/sound_module.cpp
    src/sound/audio_asset.cpp
    src/sound/miniaudio_impl.cpp
    # game
    src/game/Game.cpp
    src/game/scene.cpp
    src/game/game_object.cpp
    src/game/game_module.cpp
    # examples/doodlejump
    src/examples/doodlejump/DoodleJumpGame.cpp
    # ui
    src/ui/input_manager.cpp
    src/ui/ui_module.cpp
    # assets
    src/assets/asset_manager.cpp
    src/assets/stb_impl.cpp
    # nodes / coren
    src/nodes/coren/basic_nodes.cpp
    src/nodes/coren/convert_node.cpp
    src/nodes/coren/asset_node.cpp
    src/nodes/coren/checker_texture.cpp
    src/nodes/coren/text_texture_node.cpp
    # nodes / rendern
    src/nodes/rendern/render_nodes.cpp
    # nodes / soundn
    src/nodes/soundn/generator_nodes.cpp
    src/nodes/soundn/filter_nodes.cpp
    src/nodes/soundn/mixer_nodes.cpp
    src/nodes/soundn/io_nodes.cpp
    src/nodes/soundn/spatial_nodes.cpp
    # nodes / uin
    src/nodes/uin/ui_nodes.cpp
    src/nodes/uin/ui_render_node.cpp
)

target_include_directories(3tone PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(3tone PRIVATE Threads::Threads glfw nlohmann_json::nlohmann_json)

if(MSVC)
    target_compile_options(3tone PRIVATE /W4 /EHsc)
    target_compile_definitions(3tone PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(3tone PRIVATE -Wall -Wextra -Wpedantic)
endif()