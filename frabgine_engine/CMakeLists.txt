cmake_minimum_required(VERSION 3.20)
project(FrabgineEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(FRABGINE_BUILD_EDITOR "Build the editor application" ON)
option(FRABGINE_BUILD_TESTS "Build tests" OFF)
option(FRABGINE_ENABLE_VULKAN "Enable Vulkan rendering backend" ON)

# Core library
add_library(frabgine_core STATIC
    core/math/Vector.hpp
    core/math/Matrix.hpp
    core/math/Quaternion.hpp
    core/math/MathUtils.hpp
    core/memory/SmartPointers.hpp
    core/memory/MemoryPool.hpp
    core/serialization/Serializer.hpp
    core/serialization/JsonSerializer.cpp
    core/serialization/BinarySerializer.cpp
    core/utils/Timer.hpp
    core/utils/Logger.hpp
    core/utils/TaskScheduler.hpp
    core/utils/FileUtils.cpp
    core/utils/FileUtils.hpp
)

target_include_directories(frabgine_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Graphics library
add_library(frabgine_graphics STATIC
    graphics/Renderer.hpp
    graphics/Shader.hpp
    graphics/Texture.hpp
    graphics/Buffer.hpp
    graphics/resources/Resource.hpp
    graphics/resources/ResourceManager.hpp
    graphics/resources/ResourceManager.cpp
    graphics/vulkan/VulkanRenderer.cpp
    graphics/vulkan/VulkanRenderer.hpp
    graphics/vulkan/VulkanDevice.cpp
    graphics/vulkan/VulkanDevice.hpp
    graphics/vulkan/VulkanSwapchain.cpp
    graphics/vulkan/VulkanSwapchain.hpp
    graphics/vulkan/VulkanPipeline.cpp
    graphics/vulkan/VulkanPipeline.hpp
)

target_link_libraries(frabgine_graphics PUBLIC frabgine_core)
target_include_directories(frabgine_graphics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(FRABGINE_ENABLE_VULKAN)
    find_package(Vulkan REQUIRED)
    target_link_libraries(frabgine_graphics PUBLIC Vulkan::Vulkan)
    target_compile_definitions(frabgine_graphics PUBLIC FRABGINE_USE_VULKAN)
endif()

# Engine library
add_library(frabgine_engine STATIC
    engine/scene/Scene.hpp
    engine/scene/Scene.cpp
    engine/scene/Entity.hpp
    engine/scene/Component.hpp
    engine/physics/PhysicsWorld.hpp
    engine/physics/PhysicsWorld.cpp
    engine/audio/AudioSystem.hpp
    engine/audio/AudioSystem.cpp
    engine/animation/AnimationSystem.hpp
    engine/animation/AnimationSystem.cpp
)

target_link_libraries(frabgine_engine PUBLIC frabgine_core frabgine_graphics)
target_include_directories(frabgine_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Find PhysX
find_package(PhysX REQUIRED)
target_link_libraries(frabgine_engine PUBLIC PhysX::PhysX)

# Find OpenAL
find_package(OpenAL REQUIRED)
target_link_libraries(frabgine_engine PUBLIC OpenAL::OpenAL)

# Editor application
if(FRABGINE_BUILD_EDITOR)
    find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)
    
    add_executable(frabgine_editor
        editor/main.cpp
        editor/gui/MainWindow.hpp
        editor/gui/MainWindow.cpp
        editor/nodes/NodeGraph.hpp
        editor/nodes/NodeGraph.cpp
        editor/nodes/Node.hpp
        editor/nodes/Node.cpp
        editor/inspector/Inspector.hpp
        editor/inspector/Inspector.cpp
    )
    
    target_link_libraries(frabgine_editor PRIVATE 
        frabgine_engine 
        Qt6::Widgets 
        Qt6::OpenGLWidgets
    )
    target_include_directories(frabgine_editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# Install targets
install(TARGETS frabgine_core frabgine_graphics frabgine_engine
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

if(FRABGINE_BUILD_EDITOR)
    install(TARGETS frabgine_editor
        RUNTIME DESTINATION bin
    )
endif()
