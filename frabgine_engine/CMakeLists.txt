cmake_minimum_required(VERSION 3.20)
project(FrabgineEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(FRABGINE_BUILD_EDITOR "Build the editor application" ON)
option(FRABGINE_BUILD_TESTS "Build tests" OFF)
option(FRABGINE_ENABLE_VULKAN "Enable Vulkan rendering backend" ON)

# --------------------------------------------------------------
# Vulkan: принудительно задаём пути из переменной окружения VULKAN_SDK
# --------------------------------------------------------------
if(FRABGINE_ENABLE_VULKAN)
    # Если переменная окружения не попала в CMake, прочитаем её явно
    if(DEFINED ENV{VULKAN_SDK})
        set(VULKAN_SDK $ENV{VULKAN_SDK})
    elseif(DEFINED ENV{VK_SDK_PATH})
        set(VULKAN_SDK $ENV{VK_SDK_PATH})
    else()
        message(FATAL_ERROR "Vulkan SDK not found: set VULKAN_SDK environment variable")
    endif()

    message(STATUS "Using Vulkan SDK from: ${VULKAN_SDK}")

    # Типичные пути для include и lib (проверяем несколько вариантов)
    set(VULKAN_INCLUDE_PATH ${VULKAN_SDK}/Include)
    set(VULKAN_LIB_PATH ${VULKAN_SDK}/Lib)
    
    if(NOT EXISTS ${VULKAN_INCLUDE_PATH})
        set(VULKAN_INCLUDE_PATH ${VULKAN_SDK}/include)
    endif()
    if(NOT EXISTS ${VULKAN_LIB_PATH})
        set(VULKAN_LIB_PATH ${VULKAN_SDK}/lib)
    endif()
    # Дополнительная проверка для 64-битной версии
    if(EXISTS ${VULKAN_SDK}/Lib/x86_64)
        set(VULKAN_LIB_PATH ${VULKAN_SDK}/Lib/x86_64)
    elseif(EXISTS ${VULKAN_SDK}/lib/x86_64)
        set(VULKAN_LIB_PATH ${VULKAN_SDK}/lib/x86_64)
    endif()

    if(NOT EXISTS ${VULKAN_INCLUDE_PATH}/vulkan/vulkan.h)
        message(FATAL_ERROR "Vulkan headers not found in ${VULKAN_INCLUDE_PATH}")
    endif()
    if(NOT EXISTS ${VULKAN_LIB_PATH}/vulkan-1.lib)
        message(FATAL_ERROR "Vulkan library not found in ${VULKAN_LIB_PATH}")
    endif()

    # Создаём импортированную цель для Vulkan вручную
    add_library(Vulkan::Vulkan UNKNOWN IMPORTED)
    set_target_properties(Vulkan::Vulkan PROPERTIES
        IMPORTED_LOCATION "${VULKAN_LIB_PATH}/vulkan-1.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${VULKAN_INCLUDE_PATH}"
    )
    # Для совместимости с find_package(Vulkan) установим переменные
    set(Vulkan_INCLUDE_DIR ${VULKAN_INCLUDE_PATH})
    set(Vulkan_LIBRARY ${VULKAN_LIB_PATH}/vulkan-1.lib)
    set(Vulkan_FOUND TRUE)
endif()

# --------------------------------------------------------------
# Core library (не зависит от графики)
# --------------------------------------------------------------
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

# --------------------------------------------------------------
# Graphics library (зависит от core и Vulkan)
# --------------------------------------------------------------
add_library(frabgine_graphics STATIC
    graphics/Renderer.hpp
    graphics/Shader.hpp
    graphics/Texture.hpp
    graphics/Buffer.hpp
    graphics/resources/Resource.hpp
    graphics/resources/ResourceManager.hpp
    graphics/resources/ResourceManager.cpp
    # Vulkan файлы (будут включены только если опция включена)
)

if(FRABGINE_ENABLE_VULKAN)
    target_sources(frabgine_graphics PRIVATE
        graphics/vulkan/VulkanRenderer.cpp
        graphics/vulkan/VulkanRenderer.hpp
        graphics/vulkan/VulkanDevice.cpp
        graphics/vulkan/VulkanDevice.hpp
        graphics/vulkan/VulkanSwapchain.cpp
        graphics/vulkan/VulkanSwapchain.hpp
        graphics/vulkan/VulkanPipeline.cpp
        graphics/vulkan/VulkanPipeline.hpp
    )
    target_link_libraries(frabgine_graphics PUBLIC Vulkan::Vulkan)
    target_compile_definitions(frabgine_graphics PUBLIC FRABGINE_USE_VULKAN)
endif()

target_link_libraries(frabgine_graphics PUBLIC frabgine_core)
target_include_directories(frabgine_graphics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# --------------------------------------------------------------
# Engine library (физика, аудио, анимация)
# --------------------------------------------------------------
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

# --------------------------------------------------------------
# PhysX (опционально, но если не найден – предупреждение и отключение)
# --------------------------------------------------------------
find_package(PhysX QUIET)
if(PhysX_FOUND)
    target_link_libraries(frabgine_engine PUBLIC PhysX::PhysX)
    target_compile_definitions(frabgine_engine PUBLIC FRABGINE_USE_PHYSX)
else()
    message(WARNING "PhysX not found – physics will be disabled")
endif()

# --------------------------------------------------------------
# OpenAL (опционально)
# --------------------------------------------------------------
find_package(OpenAL QUIET)
if(OpenAL_FOUND)
    target_link_libraries(frabgine_engine PUBLIC OpenAL::OpenAL)
    target_compile_definitions(frabgine_engine PUBLIC FRABGINE_USE_OPENAL)
else()
    message(WARNING "OpenAL not found – audio will be disabled")
endif()

# --------------------------------------------------------------
# Editor (Qt6) – если не найден, редактор не собирается
# --------------------------------------------------------------
if(FRABGINE_BUILD_EDITOR)
    find_package(Qt6 COMPONENTS Widgets OpenGLWidgets QUIET)
    if(Qt6_FOUND)
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
        target_compile_definitions(frabgine_editor PRIVATE FRABGINE_EDITOR)
    else()
        message(WARNING "Qt6 not found – editor will NOT be built")
        set(FRABGINE_BUILD_EDITOR OFF)
    endif()
endif()

# --------------------------------------------------------------
# Установка
# --------------------------------------------------------------
install(TARGETS frabgine_core frabgine_graphics frabgine_engine
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

if(FRABGINE_BUILD_EDITOR AND TARGET frabgine_editor)
    install(TARGETS frabgine_editor
        RUNTIME DESTINATION bin
    )
endif()