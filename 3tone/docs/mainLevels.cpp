// main.cpp
#include "core/graph.hpp"
#include "core/executor.hpp"
#include "nodes/basic_nodes.hpp"
#include "nodes/node_factory.hpp"
#include "types/type_system.hpp"
#include <iostream>
#include <sstream>

using namespace arxglue;

// ------------------------------------------------------------
// Диагностический узел: печатает адрес контекста, вход/выход и состояние
// ------------------------------------------------------------
class DebugNode : public INode {
public:
    DebugNode(const std::string& tag, bool printState = false)
        : m_tag(tag), m_printState(printState) {}

    void execute(Context& ctx) override {
        std::cout << "[" << m_tag << "] Context: " << &ctx;
        if (getMetadata().inputs.size() > 0) {
            auto val = getInputValue(ctx, 0);
            if (val.has_value()) {
                if (val.type() == typeid(int))
                    std::cout << ", input(int): " << std::any_cast<int>(val);
                else if (val.type() == typeid(float))
                    std::cout << ", input(float): " << std::any_cast<float>(val);
            }
        }
        std::cout << std::endl;

        if (m_printState) {
            std::shared_lock lock(ctx.stateMutex);
            std::cout << "  State keys: ";
            for (const auto& [k, v] : ctx.state) {
                std::cout << k << " ";
            }
            std::cout << std::endl;
        }

        // Пробрасываем вход на выход (или генерируем число)
        if (getMetadata().inputs.empty()) {
            setOutputValue(ctx, 0, m_value);
        } else {
            setOutputValue(ctx, 0, getInputValue(ctx, 0));
        }
    }

    ComponentMetadata getMetadata() const override {
        if (m_hasInput) {
            return { "DebugNode", {{"in", typeid(int), true}}, {{"out", typeid(int)}}, true, false };
        } else {
            return { "DebugNode", {}, {{"out", typeid(int)}}, true, false };
        }
    }

    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "tag") m_tag = std::any_cast<std::string>(value);
        else if (name == "value") m_value = std::any_cast<int>(value);
        else if (name == "hasInput") m_hasInput = std::any_cast<bool>(value);
        else if (name == "printState") m_printState = std::any_cast<bool>(value);
    }

    std::any getParameter(const std::string& name) const override {
        if (name == "tag") return m_tag;
        if (name == "value") return m_value;
        if (name == "hasInput") return m_hasInput;
        if (name == "printState") return m_printState;
        return {};
    }

    void serialize(nlohmann::json& j) const override {
        j["type"] = "DebugNode";
        j["params"]["tag"] = m_tag;
        j["params"]["value"] = m_value;
        j["params"]["hasInput"] = m_hasInput;
        j["params"]["printState"] = m_printState;
    }

    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params")) {
            auto& p = j["params"];
            if (p.contains("tag")) m_tag = p["tag"];
            if (p.contains("value")) m_value = p["value"];
            if (p.contains("hasInput")) m_hasInput = p["hasInput"];
            if (p.contains("printState")) m_printState = p["printState"];
        }
    }

private:
    std::string m_tag = "Debug";
    int m_value = 0;
    bool m_hasInput = true;
    bool m_printState = false;
};

// ------------------------------------------------------------
// Тест 1: Узел с подграфом компонентов (общий контекст)
// ------------------------------------------------------------
class ComponentSubgraphNode : public INode {
public:
    void execute(Context& ctx) override {
        std::cout << "\n[ComponentSubgraphNode] addr(ctx)=" << &ctx << std::endl;

        int input = std::any_cast<int>(getInputValue(ctx, 0));

        // Компоненты работают с тем же ctx
        auto comp1 = [&](int x) {
            std::cout << "  comp1: ctx=" << &ctx << ", x=" << x << std::endl;
            ctx.setState("comp1_out", x * 2);
            return x * 2;
        };
        auto comp2 = [&](int x) {
            std::cout << "  comp2: ctx=" << &ctx << ", x=" << x << std::endl;
            ctx.setState("comp2_out", x + 10);
            return x + 10;
        };
        auto comp3 = [&](int x) {
            std::cout << "  comp3: ctx=" << &ctx << ", x=" << x << std::endl;
            return x * 3;
        };

        int v1 = comp1(input);
        int v2 = comp2(v1);
        int result = comp3(v2);

        setOutputValue(ctx, 0, result);
    }

    ComponentMetadata getMetadata() const override {
        return { "ComponentSubgraphNode", {{"in", typeid(int)}}, {{"out", typeid(int)}}, false, false };
    }
    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "ComponentSubgraphNode"; }
    void deserialize(const nlohmann::json&) override {}
};

// ------------------------------------------------------------
// Тест 2: Узел с локальным графом INode (изолированный контекст)
// ------------------------------------------------------------
class NodeSubgraphNode : public INode {
public:
    void execute(Context& globalCtx) override {
        std::cout << "\n[NodeSubgraphNode] globalCtx=" << &globalCtx << std::endl;

        int input = std::any_cast<int>(getInputValue(globalCtx, 0));
        std::cout << "  input=" << input << std::endl;

        // Создаём локальный граф
        Graph localGraph;
        // Узел-константа
        auto constNode = std::make_unique<ConstantNode<int>>(5);
        NodeId constId = localGraph.addNode(std::move(constNode));
        // Узел сложения
        auto addNode = std::make_unique<AddNode>();
        NodeId addId = localGraph.addNode(std::move(addNode));
        // Диагностический узел (чтобы увидеть адрес контекста)
        auto debugNode = std::make_unique<DebugNode>("LocalDebug", true);
        NodeId debugId = localGraph.addNode(std::move(debugNode));

        // Соединяем: входное значение нужно как-то передать.
        // Используем ConstantNode с входным значением (но это статично)
        auto inputConst = std::make_unique<ConstantNode<int>>(input);
        NodeId inputId = localGraph.addNode(std::move(inputConst));
        localGraph.addConnection({inputId, 0, addId, 0});
        localGraph.addConnection({constId, 0, addId, 1});
        localGraph.addConnection({addId, 0, debugId, 0});

        // Выполняем с локальным контекстом
        Context localCtx;
        Executor exec(1);
        exec.execute(localGraph, localCtx, {debugId});

        // Результат из локального контекста
        int result = std::any_cast<int>(localCtx.output);
        std::cout << "  local result=" << result << std::endl;

        setOutputValue(globalCtx, 0, result);
    }

    ComponentMetadata getMetadata() const override {
        return { "NodeSubgraphNode", {{"in", typeid(int)}}, {{"out", typeid(int)}}, false, false };
    }
    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "NodeSubgraphNode"; }
    void deserialize(const nlohmann::json&) override {}
};

// ------------------------------------------------------------
// Тест 3: Компонент-обёртка над INode
// ------------------------------------------------------------
// Универсальная обёртка: выполняет узел в переданном контексте.
// Предполагается, что узел не имеет входов или его входы уже подготовлены в ctx.state.
template<typename OutputType>
class NodeComponent {
public:
    explicit NodeComponent(std::unique_ptr<INode> node) : m_node(std::move(node)) {}

    OutputType operator()(Context& ctx) {
        // Выполняем узел в текущем контексте
        m_node->execute(ctx);
        // Результат должен быть в ctx.output или в выходном порту.
        // Для простоты ожидаем, что узел записывает результат в ctx.output
        // (как это делают стандартные узлы через setOutputValue)
        if (!ctx.output.has_value()) {
            throw std::runtime_error("NodeComponent: node did not set ctx.output");
        }
        return std::any_cast<OutputType>(ctx.output);
    }

private:
    std::unique_ptr<INode> m_node;
};

class WrappedNodeInComponentNode : public INode {
public:
    void execute(Context& ctx) override {
        std::cout << "\n[WrappedNodeInComponentNode] ctx=" << &ctx << std::endl;

        // Оборачиваем ConstantNode
        auto constNode = std::make_unique<ConstantNode<int>>(100);
        NodeComponent<int> constComp(std::move(constNode));
        int val = constComp(ctx);
        std::cout << "  wrapped ConstantNode -> " << val << std::endl;

        // Оборачиваем AddNode, но у AddNode два входа.
        // Входы нужно предварительно поместить в ctx.state с ключами in_<id>_0 и in_<id>_1.
        // Поскольку id узла неизвестен заранее, мы можем создать узел, временно добавить его в граф,
        // чтобы Executor подготовил state, но это сложно.
        // Вместо этого для демонстрации используем простой узел с одним входом.
        // Создадим кастомный узел, который умножает вход на 2.
        class MultiplyNode : public INode {
        public:
            void execute(Context& c) override {
                int x = std::any_cast<int>(getInputValue(c, 0));
                setOutputValue(c, 0, x * 2);
            }
            ComponentMetadata getMetadata() const override {
                return { "MultiplyNode", {{"in", typeid(int)}}, {{"out", typeid(int)}}, true, false };
            }
            void setParameter(const std::string&, const std::any&) override {}
            std::any getParameter(const std::string&) const override { return {}; }
            void serialize(nlohmann::json&) const override {}
            void deserialize(const nlohmann::json&) override {}
        };

        // Чтобы передать вход в MultiplyNode, нужно вручную положить значение в state.
        // Ключ формируется как "in_<id>_0". Узнать id можно после добавления в граф,
        // но здесь мы выполняем узел вручную, поэтому можем задать id явно.
        auto multNode = std::make_unique<MultiplyNode>();
        multNode->setId(999); // фиктивный id
        NodeComponent<int> multComp(std::move(multNode));

        // Подготавливаем входное значение в state
        ctx.setState("in_999_0", val);
        int result = multComp(ctx);
        std::cout << "  wrapped MultiplyNode(" << val << ") -> " << result << std::endl;

        setOutputValue(ctx, 0, result);
    }

    ComponentMetadata getMetadata() const override {
        return { "WrappedNodeInComponentNode", {}, {{"out", typeid(int)}}, false, false };
    }
    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "WrappedNodeInComponentNode"; }
    void deserialize(const nlohmann::json&) override {}
};

// ------------------------------------------------------------
// Тест 4: Многоуровневое чередование
// ------------------------------------------------------------
class NestedMixedNode : public INode {
public:
    void execute(Context& globalCtx) override {
        std::cout << "\n[NestedMixedNode] globalCtx=" << &globalCtx << std::endl;

        // Внутренний узел, который использует компоненты
        class InnerComponentNode : public INode {
        public:
            void execute(Context& ctx) override {
                std::cout << "  [InnerComponentNode] ctx=" << &ctx << std::endl;
                int input = std::any_cast<int>(getInputValue(ctx, 0));
                auto c1 = [](int x) { return x + 5; };
                auto c2 = [](int x) { return x * 2; };
                int res = c2(c1(input));
                setOutputValue(ctx, 0, res);
            }
            ComponentMetadata getMetadata() const override {
                return { "InnerComponentNode", {{"in", typeid(int)}}, {{"out", typeid(int)}}, true, false };
            }
            void setParameter(const std::string&, const std::any&) override {}
            std::any getParameter(const std::string&) const override { return {}; }
            void serialize(nlohmann::json&) const override {}
            void deserialize(const nlohmann::json&) override {}
        };

        // Строим локальный граф с InnerComponentNode
        Graph localGraph;
        auto inputConst = std::make_unique<ConstantNode<int>>(10);
        NodeId inputId = localGraph.addNode(std::move(inputConst));
        auto innerNode = std::make_unique<InnerComponentNode>();
        NodeId innerId = localGraph.addNode(std::move(innerNode));
        auto debugNode = std::make_unique<DebugNode>("LocalAfterInner", true);
        NodeId debugId = localGraph.addNode(std::move(debugNode));

        localGraph.addConnection({inputId, 0, innerId, 0});
        localGraph.addConnection({innerId, 0, debugId, 0});

        Context localCtx;
        Executor exec(1);
        exec.execute(localGraph, localCtx, {debugId});

        int localResult = std::any_cast<int>(localCtx.output);
        std::cout << "  local graph result: " << localResult << std::endl;

        // Применяем компонент на верхнем уровне
        auto finalComp = [&](int x) {
            std::cout << "  finalComp: ctx=" << &globalCtx << ", x=" << x << std::endl;
            return x * 3;
        };
        int finalResult = finalComp(localResult);

        setOutputValue(globalCtx, 0, finalResult);
    }

    ComponentMetadata getMetadata() const override {
        return { "NestedMixedNode", {}, {{"out", typeid(int)}}, false, false };
    }
    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "NestedMixedNode"; }
    void deserialize(const nlohmann::json&) override {}
};

// ------------------------------------------------------------
// Регистрация
// ------------------------------------------------------------
void registerTestNodes() {
    auto& f = NodeFactory::instance();
    f.registerNode("DebugNode", []() { return std::make_unique<DebugNode>("", false); });
    f.registerNode("ComponentSubgraphNode", []() { return std::make_unique<ComponentSubgraphNode>(); });
    f.registerNode("NodeSubgraphNode", []() { return std::make_unique<NodeSubgraphNode>(); });
    f.registerNode("WrappedNodeInComponentNode", []() { return std::make_unique<WrappedNodeInComponentNode>(); });
    f.registerNode("NestedMixedNode", []() { return std::make_unique<NestedMixedNode>(); });
}

// ------------------------------------------------------------
// Тесты
// ------------------------------------------------------------
void runTest1() {
    std::cout << "\n========== Test 1: Component subgraph ==========" << std::endl;
    Graph g;
    auto c = std::make_unique<ConstantNode<int>>(3);
    NodeId cid = g.addNode(std::move(c));
    auto node = std::make_unique<ComponentSubgraphNode>();
    NodeId nid = g.addNode(std::move(node));
    auto cons = std::make_unique<FloatConsumerNode>();
    NodeId outId = g.addNode(std::move(cons));

    g.addConnection({cid, 0, nid, 0});
    g.addConnection({nid, 0, outId, 0});

    Context ctx;
    Executor ex(1);
    ex.execute(g, ctx, {outId});
    std::cout << "Final output: " << std::any_cast<float>(ctx.output) << " (expected 54)" << std::endl;
}

void runTest2() {
    std::cout << "\n========== Test 2: Local INode subgraph ==========" << std::endl;
    Graph g;
    auto c = std::make_unique<ConstantNode<int>>(7);
    NodeId cid = g.addNode(std::move(c));
    auto node = std::make_unique<NodeSubgraphNode>();
    NodeId nid = g.addNode(std::move(node));
    auto cons = std::make_unique<FloatConsumerNode>();
    NodeId outId = g.addNode(std::move(cons));

    g.addConnection({cid, 0, nid, 0});
    g.addConnection({nid, 0, outId, 0});

    Context ctx;
    Executor ex(1);
    ex.execute(g, ctx, {outId});
    std::cout << "Final output: " << std::any_cast<float>(ctx.output) << " (expected 12)" << std::endl;
}

void runTest3() {
    std::cout << "\n========== Test 3: Node wrapped as Component ==========" << std::endl;
    Graph g;
    auto node = std::make_unique<WrappedNodeInComponentNode>();
    NodeId nid = g.addNode(std::move(node));
    auto cons = std::make_unique<FloatConsumerNode>();
    NodeId outId = g.addNode(std::move(cons));

    g.addConnection({nid, 0, outId, 0});

    Context ctx;
    Executor ex(1);
    ex.execute(g, ctx, {outId});
    std::cout << "Final output: " << std::any_cast<float>(ctx.output) << " (expected 200)" << std::endl;
}

void runTest4() {
    std::cout << "\n========== Test 4: Multi-level nesting ==========" << std::endl;
    Graph g;
    auto node = std::make_unique<NestedMixedNode>();
    NodeId nid = g.addNode(std::move(node));
    auto cons = std::make_unique<FloatConsumerNode>();
    NodeId outId = g.addNode(std::move(cons));

    g.addConnection({nid, 0, outId, 0});

    Context ctx;
    Executor ex(1);
    ex.execute(g, ctx, {outId});
    std::cout << "Final output: " << std::any_cast<float>(ctx.output) << " (expected 90)" << std::endl;
}

int main() {
    try {
        initBasicTypes();
        registerBasicNodes();
        registerTestNodes();

        runTest1();
        runTest2();
        runTest3();
        runTest4();

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}