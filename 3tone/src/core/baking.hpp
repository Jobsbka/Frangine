#pragma once
#include "graph.hpp"
#include <string>
#include <functional>

namespace arxglue {

/**
 * Запекает подграф, начиная с корневого узла rootId, в файл ассета по указанному пути.
 * @param graph Исходный граф.
 * @param rootId Идентификатор корневого узла подграфа.
 * @param assetPath Путь для сохранения запечённого ассета.
 * @param assetType Тип ассета ("texture" или "mesh").
 * @return Идентификатор нового узла AssetNode, заменяющего подграф, или 0 при ошибке.
 */
NodeId bakeSubgraph(Graph& graph, NodeId rootId, const std::string& assetPath, const std::string& assetType = "texture");

/**
 * Собирает все узлы, от которых зависит данный (включая сам root).
 */
std::vector<NodeId> collectDependencies(Graph& graph, NodeId rootId);

} // namespace arxglue