#pragma once

#include "core/memory/SmartPointers.hpp"
#include <string>
#include <vector>
#include <functional>
#include <any>

namespace frabgine::editor::nodes {

// Типы данных для портов узлов
enum class PortDataType {
    Float,
    Vec2,
    Vec3,
    Vec4,
    Color,
    Int,
    Bool,
    String,
    Texture,
    Material,
    Geometry
};

// Базовый класс значения порта
struct PortValue {
    virtual ~PortValue() = default;
    PortDataType type;
    
    template<typename T>
    T as() const { return std::any_cast<T>(value); }
    
    template<typename T>
    void set(const T& val) { value = val; }
    
private:
    std::any value;
};

// Порт узла (входной или выходной)
struct NodePort {
    enum class Direction { Input, Output };
    
    std::string name;
    Direction direction;
    PortDataType dataType;
    std::string label;
    
    // Для входных портов - значение по умолчанию
    std::shared_ptr<PortValue> defaultValue;
    
    // Соединения (для выходных портов)
    std::vector<std::pair<int, int>> connections; // nodeId, portIndex
    
    bool isConnected() const { return !connections.empty(); }
    void disconnectAll() { connections.clear(); }
};

// Базовый класс узла графа
class Node {
public:
    Node(int id, const std::string& name, const std::string& category = "");
    virtual ~Node() = default;
    
    // Идентификация
    int getId() const { return id_; }
    const std::string& getName() const { return name_; }
    const std::string& getCategory() const { return category_; }
    
    // Позиция в редакторе
    float getX() const { return x_; }
    float getY() const { return y_; }
    void setPosition(float x, float y) { x_ = x; y_ = y; }
    
    // Порты
    const std::vector<NodePort>& getInputPorts() const { return inputPorts_; }
    const std::vector<NodePort>& getOutputPorts() const { return outputPorts_; }
    
    NodePort* addInputPort(const std::string& name, PortDataType type, 
                          const std::string& label = "");
    NodePort* addOutputPort(const std::string& name, PortDataType type, 
                           const std::string& label = "");
    
    // Параметры узла (отображаются в инспекторе)
    template<typename T>
    void addParameter(const std::string& name, const T& defaultValue) {
        parameters_[name] = defaultValue;
    }
    
    template<typename T>
    T getParameter(const std::string& name) const {
        auto it = parameters_.find(name);
        if (it != parameters_.end()) {
            return std::any_cast<T>(it->second);
        }
        return T{};
    }
    
    template<typename T>
    void setParameter(const std::string& name, const T& value) {
        parameters_[name] = value;
    }
    
    // Выполнение узла (переопределяется в наследниках)
    virtual void execute() {}
    
    // Сериализация
    virtual std::string serialize() const;
    virtual bool deserialize(const std::string& json);
    
protected:
    int id_;
    std::string name_;
    std::string category_;
    float x_ = 0.0f;
    float y_ = 0.0f;
    
    std::vector<NodePort> inputPorts_;
    std::vector<NodePort> outputPorts_;
    std::unordered_map<std::string, std::any> parameters_;
};

// Граф узлов
class NodeGraph {
public:
    NodeGraph(const std::string& name = "NodeGraph");
    ~NodeGraph();
    
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    
    // Управление узлами
    template<typename T, typename... Args>
    T* createNode(Args&&... args) {
        static_assert(std::is_base_of<Node, T>::value, "T must inherit from Node");
        
        int id = nextNodeId_++;
        auto node = std::make_unique<T>(id, std::forward<Args>(args)...);
        T* ptr = node.get();
        nodes_.push_back(std::move(node));
        return ptr;
    }
    
    void removeNode(int nodeId);
    Node* getNode(int nodeId);
    const Node* getNode(int nodeId) const;
    
    std::vector<Node*> getAllNodes();
    std::vector<const Node*> getAllNodes() const;
    
    // Соединения
    bool connect(int fromNodeId, int fromPortIndex, int toNodeId, int toPortIndex);
    void disconnect(int fromNodeId, int fromPortIndex, int toNodeId, int toPortIndex);
    void disconnectAllFromNode(int nodeId);
    
    // Выполнение графа
    virtual void execute();
    
    // Сериализация
    std::string serialize() const;
    bool deserialize(const std::string& json);
    
    // Валидация графа (проверка на циклы и т.д.)
    bool validate() const;
    
protected:
    std::string name_;
    std::vector<std::unique_ptr<Node>> nodes_;
    int nextNodeId_ = 0;
    
    // Поиск путей для валидации
    bool hasCycle() const;
    bool hasCycleDFS(int nodeId, std::vector<bool>& visited, 
                    std::vector<bool>& recStack) const;
};

// Специализированные типы узлов для различных систем

// Узел материала
class MaterialNode : public Node {
public:
    using Node::Node;
    
    void execute() override;
    std::string serialize() const override;
    bool deserialize(const std::string& json) override;
};

// Узел текстуры
class TextureNode : public Node {
public:
    using Node::Node;
    
    void execute() override;
};

// Узел математической операции
class MathOpNode : public Node {
public:
    enum class OpType { Add, Subtract, Multiply, Divide, Power, Sqrt, Sin, Cos };
    
    using Node::Node;
    
    void execute() override;
    
private:
    OpType opType_ = OpType::Add;
};

// Узел векторной операции
class VectorOpNode : public Node {
public:
    enum class OpType { Dot, Cross, Normalize, Length, Lerp };
    
    using Node::Node;
    
    void execute() override;
    
private:
    OpType opType_ = OpType::Dot;
};

} // namespace frabgine::editor::nodes
