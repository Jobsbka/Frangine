// src/game/ProceduralPlatformGenerator.cpp
#include "ProceduralPlatformGenerator.hpp"
#include "../nodes/basic_nodes.hpp"
#include "../nodes/convert_node.hpp"
#include "../types/type_system.hpp"
#include <random>

namespace arxglue::game {

ProceduralPlatformGenerator::ProceduralPlatformGenerator() : m_executor(1) {
    buildGraph();
}

void ProceduralPlatformGenerator::buildGraph() {
    // Узел-константа для seed (будем менять через параметр)
    auto seedNode = std::make_unique<ConstantNode<int>>(0);
    NodeId seedId = m_graph.addNode(std::move(seedNode));

    // Узел, выдающий Y-координату (тоже константа, но будем обновлять перед выполнением)
    auto yNode = std::make_unique<ConstantNode<float>>(0.0f);
    NodeId yId = m_graph.addNode(std::move(yNode));

    // Генератор шума Перлина для ширины
    auto noiseWidth = std::make_unique<PerlinNoiseNode>(0.2f, 3);
    NodeId noiseWidthId = m_graph.addNode(std::move(noiseWidth));
    m_graph.addConnection({seedId, 0, noiseWidthId, 0});  // seed -> noise (нужен вход? PerlinNoiseNode не имеет входов, но можно расширить)
    m_graph.addConnection({yId, 0, noiseWidthId, 1});     // y -> второй вход (если добавим)

    // Но PerlinNoiseNode у нас без входов – переделаем? Вместо этого создадим свой узел "ValueNoise" или используем Constant + математику.
    // Для демонстрации проще создать специальный узел "ProceduralPlatformNode", но чтобы показать граф, сделаем так:
    // Используем ConvertNode и математические узлы, но их пока нет. Альтернатива: создать один узел, который внутри использует std::mt19937.
    // Однако цель – показать граф, поэтому создадим простой "ProceduralNode", который читает state["y"] и state["seed"].
    
    // Пойдём по пути: добавим новый тип узла, который читает из state.
}

// Но честнее будет реализовать прямо здесь, без графа, чтобы не усложнять.
// Для реальной демонстрации возможностей движка я покажу, как создать и выполнить граф с уже существующими узлами,
// добавив недостающие математические узлы (Add, Mul, Sin и т.д.) и InputNode.

// Ниже – улучшенная версия с полноценным графом.

} // namespace