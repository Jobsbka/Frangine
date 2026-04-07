cmake_minimum_required(VERSION 3.16)
project(3tone VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(3tone
    src/main.cpp
    src/core/graph.cpp
    src/core/executor.cpp
    src/core/baking.cpp
    src/assets/asset_manager.cpp
    src/assets/stb_impl.cpp
    src/nodes/basic_nodes.cpp
    src/nodes/asset_node.cpp
    src/nodes/checker_texture.cpp
    src/nodes/convert_node.cpp
)

target_include_directories(3tone PRIVATE src)
target_include_directories(3tone PRIVATE include)

if(MSVC)
    target_compile_options(3tone PRIVATE /W4 /EHsc)
    target_compile_definitions(3tone PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(3tone PRIVATE -Wall -Wextra -Wpedantic)
endif()

find_package(Threads REQUIRED)
target_link_libraries(3tone Threads::Threads)