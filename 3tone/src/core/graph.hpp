// src/core/graph.hpp
#pragma once
#include "node.hpp"
#include <vector>
#include <unordered_map>
#include <memory>

namespace arxglue {

struct Connection {
    NodeId srcNode;
    int srcPort;
    NodeId dstNode;
    int dstPort;
};

class Graph {
public:
    NodeId addNode(std::unique_ptr<INode> node);
    void removeNode(NodeId id);
    INode* getNode(NodeId id);
    const std::vector<std::unique_ptr<INode>>& getNodes() const { return m_nodes; }

    // Добавляет соединение, при необходимости вставляя конвертер типов.
    // Бросает исключение при несовместимых типах или возникновении цикла.
    void addConnection(const Connection& conn);
    void removeConnection(NodeId src, int srcPort, NodeId dst, int dstPort);
    const std::vector<Connection>& getConnections() const { return m_connections; }

    std::vector<NodeId> getDependencies(NodeId id) const;
    std::vector<NodeId> getDependents(NodeId id) const;
    void invalidateSubgraph(NodeId root);
    std::vector<NodeId> topologicalSort() const;

    void serialize(nlohmann::json& j) const;
    void deserialize(const nlohmann::json& j);

private:
    std::vector<std::unique_ptr<INode>> m_nodes;
    std::unordered_map<NodeId, size_t> m_idToIndex;
    std::vector<Connection> m_connections;
    NodeId m_nextId = 1;

    mutable std::vector<std::vector<NodeId>> m_adj;
    mutable std::vector<int> m_inDegree;
    mutable bool m_adjValid = false;

    void buildAdjacency() const;

    // Структура для хранения результата подготовки соединения
    struct ConnectionPlan {
        std::vector<Connection> connections;   // соединения, которые нужно добавить
        NodeId createdConverterId = 0;         // ID созданного конвертера, если был создан
        bool valid = false;
    };

    ConnectionPlan prepareConnection(NodeId srcNode, int srcPort, NodeId dstNode, int dstPort);
};

} // namespace arxglue