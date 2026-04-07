// src/core/graph.cpp
#include "graph.hpp"
#include "../nodes/node_factory.hpp"
#include "../nodes/convert_node.hpp"
#include "../types/type_system.hpp"
#include <queue>
#include <algorithm>
#include <stdexcept>

namespace arxglue {

NodeId Graph::addNode(std::unique_ptr<INode> node) {
    NodeId id = m_nextId++;
    node->setId(id);
    m_idToIndex[id] = m_nodes.size();
    m_nodes.push_back(std::move(node));
    m_adjValid = false;
    return id;
}

void Graph::removeNode(NodeId id) {
    auto it = m_idToIndex.find(id);
    if (it == m_idToIndex.end()) return;
    size_t idx = it->second;
    m_nodes.erase(m_nodes.begin() + idx);
    m_idToIndex.clear();
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        m_idToIndex[m_nodes[i]->getId()] = i;
    }
    m_connections.erase(std::remove_if(m_connections.begin(), m_connections.end(),
        [id](const Connection& c) { return c.srcNode == id || c.dstNode == id; }),
        m_connections.end());
    m_adjValid = false;
}

INode* Graph::getNode(NodeId id) {
    auto it = m_idToIndex.find(id);
    if (it == m_idToIndex.end()) return nullptr;
    return m_nodes[it->second].get();
}

Graph::ConnectionPlan Graph::prepareConnection(NodeId srcNode, int srcPort, NodeId dstNode, int dstPort) {
    ConnectionPlan plan;
    plan.valid = false;

    INode* src = getNode(srcNode);
    INode* dst = getNode(dstNode);
    if (!src || !dst) return plan;

    std::type_index srcType = src->getOutputPortType(srcPort);
    std::type_index dstType = dst->getInputPortType(dstPort);

    // Если типы не указаны, разрешаем прямое соединение
    if (srcType == typeid(void) || dstType == typeid(void)) {
        plan.connections.push_back({srcNode, srcPort, dstNode, dstPort});
        plan.valid = true;
        return plan;
    }

    if (srcType == dstType) {
        plan.connections.push_back({srcNode, srcPort, dstNode, dstPort});
        plan.valid = true;
        return plan;
    }

    TypeId srcId = TypeSystem::instance().getTypeId(srcType);
    TypeId dstId = TypeSystem::instance().getTypeId(dstType);
    if (srcId == TypeId::Unknown || dstId == TypeId::Unknown) {
        return plan; // неизвестные типы
    }

    if (!TypeSystem::instance().canConvert(srcId, dstId)) {
        return plan; // конвертация невозможна
    }

    // Создаём узел-конвертер и добавляем его в граф
    auto converter = std::make_unique<ConvertNode>(srcId, dstId);
    NodeId converterId = addNode(std::move(converter));
    plan.createdConverterId = converterId;

    // Формируем два соединения
    plan.connections.push_back({srcNode, srcPort, converterId, 0});
    plan.connections.push_back({converterId, 0, dstNode, dstPort});
    plan.valid = true;
    return plan;
}

void Graph::addConnection(const Connection& conn) {
    // 1. Подготавливаем соединения (возможно, с конвертером)
    ConnectionPlan plan = prepareConnection(conn.srcNode, conn.srcPort, conn.dstNode, conn.dstPort);
    if (!plan.valid) {
        throw std::runtime_error("Incompatible port types and no conversion available");
    }

    // 2. Временно добавляем соединения
    size_t oldConnCount = m_connections.size();
    m_connections.insert(m_connections.end(), plan.connections.begin(), plan.connections.end());
    m_adjValid = false;

    // 3. Проверяем наличие циклов
    try {
        topologicalSort();
    } catch (const std::runtime_error&) {
        // Откат: удаляем добавленные соединения
        m_connections.resize(oldConnCount);
        // Если был создан конвертер, удаляем его
        if (plan.createdConverterId != 0) {
            removeNode(plan.createdConverterId);
        }
        m_adjValid = false;
        throw std::runtime_error("Connection would create a cycle");
    }
}

void Graph::removeConnection(NodeId src, int srcPort, NodeId dst, int dstPort) {
    auto it = std::find_if(m_connections.begin(), m_connections.end(),
        [=](const Connection& c) {
            return c.srcNode == src && c.srcPort == srcPort &&
                   c.dstNode == dst && c.dstPort == dstPort;
        });
    if (it != m_connections.end()) {
        m_connections.erase(it);
        m_adjValid = false;
    }
}

std::vector<NodeId> Graph::getDependencies(NodeId id) const {
    std::vector<NodeId> deps;
    for (const auto& conn : m_connections) {
        if (conn.dstNode == id) deps.push_back(conn.srcNode);
    }
    return deps;
}

std::vector<NodeId> Graph::getDependents(NodeId id) const {
    std::vector<NodeId> deps;
    for (const auto& conn : m_connections) {
        if (conn.srcNode == id) deps.push_back(conn.dstNode);
    }
    return deps;
}

void Graph::invalidateSubgraph(NodeId root) {
    std::queue<NodeId> q;
    q.push(root);
    while (!q.empty()) {
        NodeId id = q.front(); q.pop();
        INode* node = getNode(id);
        if (node && node->isDirty()) continue;
        if (node) node->setDirty(true);
        for (NodeId dep : getDependents(id)) {
            q.push(dep);
        }
    }
}

void Graph::buildAdjacency() const {
    if (m_adjValid) return;
    size_t n = m_nodes.size();
    m_adj.assign(n, {});
    m_inDegree.assign(n, 0);
    for (const auto& conn : m_connections) {
        auto srcIt = m_idToIndex.find(conn.srcNode);
        auto dstIt = m_idToIndex.find(conn.dstNode);
        if (srcIt == m_idToIndex.end() || dstIt == m_idToIndex.end()) continue;
        m_adj[srcIt->second].push_back(static_cast<NodeId>(dstIt->second));
        m_inDegree[dstIt->second]++;
    }
    m_adjValid = true;
}

std::vector<NodeId> Graph::topologicalSort() const {
    buildAdjacency();
    size_t n = m_nodes.size();
    std::vector<int> inDegree = m_inDegree;
    std::queue<size_t> q;
    for (size_t i = 0; i < n; ++i) {
        if (inDegree[i] == 0) q.push(i);
    }
    std::vector<NodeId> result;
    while (!q.empty()) {
        size_t idx = q.front(); q.pop();
        result.push_back(m_nodes[idx]->getId());
        for (size_t neighbor : m_adj[idx]) {
            if (--inDegree[neighbor] == 0) q.push(neighbor);
        }
    }
    if (result.size() != n) {
        throw std::runtime_error("Graph contains a cycle");
    }
    return result;
}

void Graph::serialize(nlohmann::json& j) const {
    j["version"] = 1;
    nlohmann::json nodesJson = nlohmann::json::array();
    for (const auto& node : m_nodes) {
        nlohmann::json nodeJson;
        node->serialize(nodeJson);
        nodeJson["id"] = node->getId();
        nodesJson.push_back(nodeJson);
    }
    j["nodes"] = nodesJson;

    nlohmann::json connsJson = nlohmann::json::array();
    for (const auto& conn : m_connections) {
        connsJson.push_back({
            {"src", conn.srcNode},
            {"srcPort", conn.srcPort},
            {"dst", conn.dstNode},
            {"dstPort", conn.dstPort}
        });
    }
    j["connections"] = connsJson;
}

void Graph::deserialize(const nlohmann::json& j) {
    m_nodes.clear();
    m_idToIndex.clear();
    m_connections.clear();
    m_nextId = 1;
    m_adjValid = false;

    std::unordered_map<NodeId, NodeId> oldToNew;
    for (const auto& nodeJson : j["nodes"]) {
        std::string type = nodeJson["type"];
        auto node = NodeFactory::instance().createNode(type);
        if (!node) continue;
        node->deserialize(nodeJson);
        NodeId newId = addNode(std::move(node));
        oldToNew[nodeJson["id"]] = newId;
    }
    for (const auto& connJson : j["connections"]) {
        Connection conn;
        conn.srcNode = oldToNew[connJson["src"]];
        conn.srcPort = connJson["srcPort"];
        conn.dstNode = oldToNew[connJson["dst"]];
        conn.dstPort = connJson["dstPort"];
        addConnection(conn);
    }
}

} // namespace arxglue