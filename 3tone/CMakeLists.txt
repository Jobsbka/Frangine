cmake_minimum_required(VERSION 3.16)
project(3tone VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(3tone
    src/main.cpp
    src/core/graph.cpp
    src/core/executor.cpp
    src/assets/asset_manager.cpp
    src/nodes/basic_nodes.cpp
)

target_include_directories(3tone PRIVATE src)
target_include_directories(3tone PRIVATE include)   # здесь лежит папка nlohmann

if(MSVC)
    target_compile_options(3tone PRIVATE /W4 /EHsc)
else()
    target_compile_options(3tone PRIVATE -Wall -Wextra -Wpedantic)
endif()

find_package(Threads REQUIRED)
target_link_libraries(3tone Threads::Threads)

# Если используете header-only nlohmann/json, достаточно указать include
# Если же хотите через find_package, раскомментируйте:
# find_package(nlohmann_json REQUIRED)
# target_link_libraries(3tone Threads::Threads nlohmann_json::nlohmann_json)