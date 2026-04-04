#include "NodeGraph.hpp"
#include "core/serialization/JsonSerializer.hpp"
#include "core/utils/Logger.hpp"
#include <algorithm>
#include <queue>

namespace frabgine::editor::nodes {

// Node implementation
Node::Node(int id, const std::string& name, const std::string& category)
    : id_(id), name_(name), category_(category) {}

NodePort* Node::addInputPort(const std::string& name, PortDataType type, const std::string& label) {
    NodePort port;
    port.name = name;
    port.direction = NodePort::Direction::Input;
    port.dataType = type;
    port.label = label.empty() ? name : label;
    inputPorts_.push_back(port);
    return &inputPorts_.back();
}

NodePort* Node::addOutputPort(const std::string& name, PortDataType type, const std::string& label) {
    NodePort port;
    port.name = name;
    port.direction = NodePort::Direction::Output;
    port.dataType = type;
    port.label = label.empty() ? name : label;
    outputPorts_.push_back(port);
    return &outputPorts_.back();
}

std::string Node::serialize() const {
    // Базовая сериализация узла
    nlohmann::json json;
    json["id"] = id_;
    json["name"] = name_;
    json["category"] = category_;
    json["x"] = x_;
    json["y"] = y_;
    
    // Сериализация параметров
    // Примечание: std::any требует специальной обработки
    // В полной реализации нужно использовать visitor pattern
    
    return json.dump(2);
}

bool Node::deserialize(const std::string& jsonStr) {
    try {
        auto json = nlohmann::json::parse(jsonStr);
        
        id_ = json.value("id", 0);
        name_ = json.value("name", "Node");
        category_ = json.value("category", "");
        x_ = json.value("x", 0.0f);
        y_ = json.value("y", 0.0f);
        
        return true;
    } catch (const std::exception& e) {
        FRABGINE_LOG_ERROR("Failed to deserialize node: {}", e.what());
        return false;
    }
}

// NodeGraph implementation
NodeGraph::NodeGraph(const std::string& name) : name_(name) {}

NodeGraph::~NodeGraph() = default;

void NodeGraph::removeNode(int nodeId) {
    auto it = std::find_if(nodes_.begin(), nodes_.end(),
        [nodeId](const std::unique_ptr<Node>& node) {
            return node->getId() == nodeId;
        });
    
    if (it != nodes_.end()) {
        // Отключаем все соединения
        disconnectAllFromNode(nodeId);
        nodes_.erase(it);
        FRABGINE_LOG_DEBUG("Removed node {} from graph {}", nodeId, name_);
    }
}

Node* NodeGraph::getNode(int nodeId) {
    for (auto& node : nodes_) {
        if (node->getId() == nodeId) {
            return node.get();
        }
    }
    return nullptr;
}

const Node* NodeGraph::getNode(int nodeId) const {
    for (const auto& node : nodes_) {
        if (node->getId() == nodeId) {
            return node.get();
        }
    }
    return nullptr;
}

std::vector<Node*> NodeGraph::getAllNodes() {
    std::vector<Node*> result;
    result.reserve(nodes_.size());
    for (auto& node : nodes_) {
        result.push_back(node.get());
    }
    return result;
}

std::vector<const Node*> NodeGraph::getAllNodes() const {
    std::vector<const Node*> result;
    result.reserve(nodes_.size());
    for (const auto& node : nodes_) {
        result.push_back(node.get());
    }
    return result;
}

bool NodeGraph::connect(int fromNodeId, int fromPortIndex, int toNodeId, int toPortIndex) {
    auto fromNode = getNode(fromNodeId);
    auto toNode = getNode(toNodeId);
    
    if (!fromNode || !toNode) {
        FRABGINE_LOG_ERROR("Invalid node IDs for connection");
        return false;
    }
    
    auto& outputPorts = fromNode->getOutputPorts();
    auto& inputPorts = toNode->getInputPorts();
    
    if (fromPortIndex < 0 || fromPortIndex >= static_cast<int>(outputPorts.size())) {
        FRABGINE_LOG_ERROR("Invalid output port index");
        return false;
    }
    
    if (toPortIndex < 0 || toPortIndex >= static_cast<int>(inputPorts.size())) {
        FRABGINE_LOG_ERROR("Invalid input port index");
        return false;
    }
    
    // Проверка типов данных
    if (outputPorts[fromPortIndex].dataType != inputPorts[toPortIndex].dataType) {
        FRABGINE_LOG_ERROR("Port data types do not match");
        return false;
    }
    
    // Добавляем соединение
    auto& outputPort = const_cast<NodePort&>(outputPorts[fromPortIndex]);
    outputPort.connections.emplace_back(toNodeId, toPortIndex);
    
    FRABGINE_LOG_DEBUG("Connected {}:{} -> {}:{}", 
                      fromNodeId, fromPortIndex, toNodeId, toPortIndex);
    
    return true;
}

void NodeGraph::disconnect(int fromNodeId, int fromPortIndex, int toNodeId, int toPortIndex) {
    auto fromNode = getNode(fromNodeId);
    if (!fromNode) return;
    
    auto& outputPorts = fromNode->getOutputPorts();
    if (fromPortIndex < 0 || fromPortIndex >= static_cast<int>(outputPorts.size())) {
        return;
    }
    
    auto& outputPort = const_cast<NodePort&>(outputPorts[fromPortIndex]);
    auto it = std::find_if(outputPort.connections.begin(), outputPort.connections.end(),
        [toNodeId, toPortIndex](const std::pair<int, int>& conn) {
            return conn.first == toNodeId && conn.second == toPortIndex;
        });
    
    if (it != outputPort.connections.end()) {
        outputPort.connections.erase(it);
        FRABGINE_LOG_DEBUG("Disconnected {}:{} -> {}:{}", 
                          fromNodeId, fromPortIndex, toNodeId, toPortIndex);
    }
}

void NodeGraph::disconnectAllFromNode(int nodeId) {
    // Удаляем все исходящие соединения
    auto node = getNode(nodeId);
    if (!node) return;
    
    for (auto& outputPort : const_cast<std::vector<NodePort>&>(node->getOutputPorts())) {
        outputPort.disconnectAll();
    }
    
    // Удаляем все входящие соединения из других узлов
    for (auto& otherNode : nodes_) {
        if (otherNode->getId() == nodeId) continue;
        
        for (auto& outputPort : const_cast<std::vector<NodePort>&>(otherNode->getOutputPorts())) {
            auto it = std::remove_if(outputPort.connections.begin(), outputPort.connections.end(),
                [nodeId](const std::pair<int, int>& conn) {
                    return conn.first == nodeId;
                });
            outputPort.connections.erase(it, outputPort.connections.end());
        }
    }
}

void NodeGraph::execute() {
    if (!validate()) {
        FRABGINE_LOG_ERROR("Cannot execute invalid graph");
        return;
    }
    
    // Топологическая сортировка и выполнение
    std::vector<int> executionOrder;
    std::unordered_map<int, int> inDegree;
    std::queue<int> zeroInDegree;
    
    // Вычисляем входящие степени
    for (const auto& node : nodes_) {
        inDegree[node->getId()] = 0;
    }
    
    for (const auto& node : nodes_) {
        for (const auto& outputPort : node->getOutputPorts()) {
            for (const auto& conn : outputPort.connections) {
                inDegree[conn.first]++;
            }
        }
    }
    
    // Находим узлы с нулевой входящей степенью
    for (const auto& [id, degree] : inDegree) {
        if (degree == 0) {
            zeroInDegree.push(id);
        }
    }
    
    // Топологическая сортировка
    while (!zeroInDegree.empty()) {
        int currentId = zeroInDegree.front();
        zeroInDegree.pop();
        executionOrder.push_back(currentId);
        
        auto node = getNode(currentId);
        if (!node) continue;
        
        for (const auto& outputPort : node->getOutputPorts()) {
            for (const auto& conn : outputPort.connections) {
                inDegree[conn.first]--;
                if (inDegree[conn.first] == 0) {
                    zeroInDegree.push(conn.first);
                }
            }
        }
    }
    
    // Выполняем узлы в порядке топологической сортировки
    for (int nodeId : executionOrder) {
        auto node = getNode(nodeId);
        if (node) {
            node->execute();
        }
    }
    
    FRABGINE_LOG_DEBUG("Executed node graph with {} nodes", executionOrder.size());
}

std::string NodeGraph::serialize() const {
    nlohmann::json json;
    json["name"] = name_;
    
    nlohmann::json nodesJson = nlohmann::json::array();
    for (const auto& node : nodes_) {
        nodesJson.push_back(nlohmann::json::parse(node->serialize()));
    }
    json["nodes"] = nodesJson;
    
    // Сериализация соединений
    nlohmann::json connectionsJson = nlohmann::json::array();
    for (const auto& node : nodes_) {
        for (const auto& outputPort : node->getOutputPorts()) {
            for (const auto& conn : outputPort.connections) {
                nlohmann::json connJson;
                connJson["fromNode"] = node->getId();
                connJson["fromPort"] = &outputPort - &node->getOutputPorts()[0];
                connJson["toNode"] = conn.first;
                connJson["toPort"] = conn.second;
                connectionsJson.push_back(connJson);
            }
        }
    }
    json["connections"] = connectionsJson;
    
    return json.dump(2);
}

bool NodeGraph::deserialize(const std::string& jsonStr) {
    try {
        auto json = nlohmann::json::parse(jsonStr);
        
        name_ = json.value("name", "NodeGraph");
        
        // Десериализация узлов будет реализована в полной версии
        // Требуется фабрика узлов для создания правильных типов
        
        return true;
    } catch (const std::exception& e) {
        FRABGINE_LOG_ERROR("Failed to deserialize node graph: {}", e.what());
        return false;
    }
}

bool NodeGraph::validate() const {
    if (hasCycle()) {
        FRABGINE_LOG_ERROR("Node graph contains cycles");
        return false;
    }
    
    // Дополнительная валидация (проверка типов портов и т.д.)
    return true;
}

bool NodeGraph::hasCycle() const {
    std::vector<bool> visited(nodes_.size(), false);
    std::vector<bool> recStack(nodes_.size(), false);
    
    for (size_t i = 0; i < nodes_.size(); i++) {
        if (!visited[i]) {
            if (hasCycleDFS(i, visited, recStack)) {
                return true;
            }
        }
    }
    
    return false;
}

bool NodeGraph::hasCycleDFS(int nodeId, std::vector<bool>& visited, 
                           std::vector<bool>& recStack) const {
    if (nodeId < 0 || nodeId >= static_cast<int>(nodes_.size())) {
        return false;
    }
    
    visited[nodeId] = true;
    recStack[nodeId] = true;
    
    const auto& node = nodes_[nodeId];
    for (const auto& outputPort : node->getOutputPorts()) {
        for (const auto& conn : outputPort.connections) {
            int nextNodeId = conn.first;
            if (nextNodeId >= 0 && nextNodeId < static_cast<int>(nodes_.size())) {
                if (!visited[nextNodeId]) {
                    if (hasCycleDFS(nextNodeId, visited, recStack)) {
                        return true;
                    }
                } else if (recStack[nextNodeId]) {
                    return true;
                }
            }
        }
    }
    
    recStack[nodeId] = false;
    return false;
}

// MaterialNode implementation
void MaterialNode::execute() {
    // Выполнение узла материала
    // В полной версии здесь будет компиляция шейдера или вычисления
}

std::string MaterialNode::serialize() const {
    std::string base = Node::serialize();
    // Добавить специфичные для материала данные
    return base;
}

bool MaterialNode::deserialize(const std::string& json) {
    return Node::deserialize(json);
}

// TextureNode implementation
void TextureNode::execute() {
    // Загрузка и обработка текстуры
}

// MathOpNode implementation
void MathOpNode::execute() {
    // Выполнение математической операции
    // Получение входных значений, вычисление, установка выходного значения
}

// VectorOpNode implementation
void VectorOpNode::execute() {
    // Выполнение векторной операции
}

} // namespace frabgine::editor::nodes
