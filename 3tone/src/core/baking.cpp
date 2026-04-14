#include "baking.hpp"
#include "executor.hpp"
#include "../nodes/coren/asset_node.hpp"
#include "../nodes/node_factory.hpp"
#include "../assets/asset_manager.hpp"
#include <unordered_set>
#include <queue>
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace arxglue {

std::vector<NodeId> collectDependencies(Graph& graph, NodeId rootId) {
    std::unordered_set<NodeId> visited;
    std::queue<NodeId> queue;
    queue.push(rootId);
    visited.insert(rootId);

    while (!queue.empty()) {
        NodeId current = queue.front();
        queue.pop();

        for (NodeId dep : graph.getDependencies(current)) {
            if (visited.find(dep) == visited.end()) {
                visited.insert(dep);
                queue.push(dep);
            }
        }
    }

    std::vector<NodeId> sortedAll = graph.topologicalSort();
    std::vector<NodeId> result;
    for (NodeId id : sortedAll) {
        if (visited.count(id)) {
            result.push_back(id);
        }
    }
    return result;
}

static bool isUsedOutside(Graph& graph, NodeId nodeId, const std::unordered_set<NodeId>& subgraphSet) {
    for (NodeId dep : graph.getDependents(nodeId)) {
        if (subgraphSet.find(dep) == subgraphSet.end()) {
            return true;
        }
    }
    return false;
}

NodeId bakeSubgraph(Graph& graph, NodeId rootId, const std::string& assetPath, const std::string& assetType) {
    // 1. Собрать зависимости
    std::vector<NodeId> subgraphNodesVec = collectDependencies(graph, rootId);
    if (subgraphNodesVec.empty()) {
        throw std::runtime_error("Bake subgraph: no nodes found");
    }

    std::unordered_set<NodeId> subgraphSet(subgraphNodesVec.begin(), subgraphNodesVec.end());

    // 2. Создать временный граф
    Graph tempGraph;
    std::unordered_map<NodeId, NodeId> oldToNew;

    for (NodeId oldId : subgraphNodesVec) {
        INode* original = graph.getNode(oldId);
        if (!original) continue;

        nlohmann::json j;
        original->serialize(j);

        // Проверяем наличие поля "type"
        if (!j.contains("type") || !j["type"].is_string()) {
            throw std::runtime_error("Serialized node missing 'type' field");
        }
        std::string type = j["type"].get<std::string>();

        auto newNode = NodeFactory::instance().createNode(type);
        if (!newNode) {
            throw std::runtime_error("NodeFactory failed to create node of type: " + type);
        }

        newNode->deserialize(j);
        NodeId newId = tempGraph.addNode(std::move(newNode));
        oldToNew[oldId] = newId;
    }

    // Копируем соединения
    for (const auto& conn : graph.getConnections()) {
        if (oldToNew.count(conn.srcNode) && oldToNew.count(conn.dstNode)) {
            Connection newConn;
            newConn.srcNode = oldToNew[conn.srcNode];
            newConn.srcPort = conn.srcPort;
            newConn.dstNode = oldToNew[conn.dstNode];
            newConn.dstPort = conn.dstPort;
            try {
                tempGraph.addConnection(newConn);
            } catch (const std::runtime_error&) {
                // Игнорируем ошибку цикла, если вдруг возникла
            }
        }
    }

    // Проверяем, что корневой узел есть во временном графе
    auto it = oldToNew.find(rootId);
    if (it == oldToNew.end()) {
        throw std::runtime_error("Root node not found in temporary graph");
    }
    NodeId tempRoot = it->second;

    // 3. Выполнить временный граф
    Executor executor(1);
    Context ctx;
    executor.execute(tempGraph, ctx, {tempRoot});

    if (!ctx.output.has_value()) {
        throw std::runtime_error("Bake subgraph produced no output");
    }

    std::any result = ctx.output;
    std::cout << "[Bake] Temporary graph output type: " << result.type().name() << std::endl;

    try {
        if (assetType == "texture") {
            auto tex = std::any_cast<std::shared_ptr<TextureAsset>>(result);
            if (!tex) {
                throw std::runtime_error("Bake result texture is null");
            }
            AssetManager::instance().saveAsset(assetPath, tex);
        } else if (assetType == "mesh") {
            auto mesh = std::any_cast<std::shared_ptr<MeshAsset>>(result);
            if (!mesh) {
                throw std::runtime_error("Bake result mesh is null");
            }
            AssetManager::instance().saveAsset(assetPath, mesh);
        } else {
            throw std::runtime_error("Unsupported asset type for baking");
        }
    } catch (const std::bad_any_cast&) {
        std::string actualType = result.type().name();
        throw std::runtime_error(std::string("Bad any_cast in bakeSubgraph: expected ") +
                                 (assetType == "texture" ? "TextureAsset" : "MeshAsset") +
                                 ", got type: " + actualType);
    }

    // 5. Замена подграфа
    auto assetNode = std::make_unique<AssetNode>(assetPath, assetType);
    NodeId assetId = graph.addNode(std::move(assetNode));

    // Перенаправляем соединения от корневого узла
    std::vector<Connection> oldConnections = graph.getConnections();
    for (const auto& conn : oldConnections) {
        if (conn.srcNode == rootId) {
            graph.removeConnection(conn.srcNode, conn.srcPort, conn.dstNode, conn.dstPort);
            Connection newConn;
            newConn.srcNode = assetId;
            newConn.srcPort = 0;
            newConn.dstNode = conn.dstNode;
            newConn.dstPort = conn.dstPort;
            graph.addConnection(newConn);
        }
    }

    // Удаляем узлы подграфа, если они не используются снаружи
    for (NodeId id : subgraphNodesVec) {
        if (!isUsedOutside(graph, id, subgraphSet)) {
            graph.removeNode(id);
        }
    }

    return assetId;
}

} // namespace arxglue