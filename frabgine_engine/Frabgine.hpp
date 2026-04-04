#pragma once

/**
 * @file Frabgine.hpp
 * @brief Главный заголовочный файл движка Frabgine
 * 
 * Подключает все основные модули движка для удобного использования
 */

// Core модули
#include "core/math/Vector.hpp"
#include "core/math/Matrix.hpp"
#include "core/math/Quaternion.hpp"
#include "core/math/MathUtils.hpp"

#include "core/memory/SmartPointers.hpp"
#include "core/memory/MemoryPool.hpp"

#include "core/serialization/Serializer.hpp"
#include "core/serialization/JsonSerializer.hpp"
#include "core/serialization/BinarySerializer.hpp"

#include "core/utils/Timer.hpp"
#include "core/utils/Logger.hpp"
#include "core/utils/TaskScheduler.hpp"
#include "core/utils/FileUtils.hpp"

// Graphics модули
#ifdef FRABGINE_ENABLE_VULKAN
#include "graphics/vulkan/VulkanRenderer.hpp"
#endif

// Engine модули
#include "engine/scene/SceneGraph.hpp"

// Editor модули (только при сборке редактора)
#ifdef FRABGINE_EDITOR
#include "editor/nodes/NodeGraph.hpp"
#endif

/**
 * @namespace frabgine
 * @brief Корневое пространство имен движка Frabgine
 */
namespace frabgine {

/**
 * @brief Инициализация движка
 * @return true если успешно
 */
inline bool initialize() {
    FRABGINE_LOG_INFO("Initializing Frabgine Engine v{}", FRABGINE_VERSION);
    return true;
}

/**
 * @brief Остановка движка
 */
inline void shutdown() {
    FRABGINE_LOG_INFO("Shutting down Frabgine Engine");
}

// Версия движка
constexpr int VERSION_MAJOR = 0;
constexpr int VERSION_MINOR = 1;
constexpr int VERSION_PATCH = 0;
constexpr const char* VERSION_STRING = "0.1.0";

} // namespace frabgine

#define FRABGINE_VERSION frabgine::VERSION_STRING
#define FRABGINE_VERSION_MAJOR frabgine::VERSION_MAJOR
#define FRABGINE_VERSION_MINOR frabgine::VERSION_MINOR
#define FRABGINE_VERSION_PATCH frabgine::VERSION_PATCH
