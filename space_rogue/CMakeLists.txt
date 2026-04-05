cmake_minimum_required(VERSION 3.20)
project(SpaceRogue VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Находим GLFW и Vulkan
find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(space_rogue
    src/main.cpp
    src/render/vulkan_context.cpp
    src/render/shader_compiler.cpp
)

target_include_directories(space_rogue PRIVATE src)

target_link_libraries(space_rogue
    glfw
    Vulkan::Vulkan
)

# Копируем шейдеры в папку сборки (для удобства)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/shaders/)