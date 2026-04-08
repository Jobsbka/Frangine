// src/game/ProceduralPlatformGenerator.hpp
#pragma once

#include "../core/graph.hpp"
#include "../core/executor.hpp"
#include "../assets/asset_manager.hpp"
#include <memory>
#include <array>

namespace arxglue::game {

struct PlatformParams {
    float width;
    float height;
    float depth;
    std::array<float, 3> color;
    bool isBouncy;      // пружинистая платформа
    bool isBreakable;   // ломающаяся
};

class ProceduralPlatformGenerator {
public:
    ProceduralPlatformGenerator();
    ~ProceduralPlatformGenerator() = default;

    // Генерирует параметры платформы по её Y-координате (и глобальному seed)
    PlatformParams generate(float y, int seedOffset = 0);

private:
    Graph m_graph;
    Executor m_executor;
    NodeId m_rootNodeId;
    Context m_ctx;

    void buildGraph();
};

} // namespace arxglue::game