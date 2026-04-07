#include "graph.hpp"
#include "../nodes/node_factory.hpp"
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

void Graph::addConnection(const Connection& conn) {
    if (!getNode(conn.srcNode) || !getNode(conn.dstNode)) return;
    m_connections.push_back(conn);
    m_adjValid = false;
    try {
        topologicalSort();
    } catch (const std::runtime_error&) {
        m_connections.pop_back();
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