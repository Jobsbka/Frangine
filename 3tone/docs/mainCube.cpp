// src/main.cpp
#include "core/graph.hpp"
#include "core/executor.hpp"
#include "core/baking.hpp"
#include "nodes/basic_nodes.hpp"
#include "nodes/checker_texture.hpp"
#include "nodes/asset_node.hpp"
#include "nodes/node_factory.hpp"
#include "nodes/render_nodes.hpp"
#include "types/type_system.hpp"
#include "assets/asset_manager.hpp"
#include "render/graphics_device.hpp"
#include <iostream>
#include <exception>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <mutex>
#include <fstream>
#include <array>
#include <cmath>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

using namespace arxglue;

// ========== Вспомогательные функции и узлы из теста графа ==========

class CountingNode : public INode {
public:
    CountingNode(const std::string& name = "") : m_name(name) {}

    void execute(Context& ctx) override {
        ++s_totalExecutions;
        int val = std::any_cast<int>(getInputValue(ctx, 0));
        int result = val * 2;
        setOutputValue(ctx, 0, result);

        static std::mutex coutMutex;
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "[CountingNode " << m_name << "] executed, total: " << s_totalExecutions.load() << std::endl;
    }

    ComponentMetadata getMetadata() const override {
        return {"CountingNode", {{"in", typeid(int), true}}, {{"out", typeid(int)}}, true, false};
    }

    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override {
        j["type"] = "CountingNode";
        j["params"]["name"] = m_name;
    }
    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params") && j["params"].contains("name"))
            m_name = j["params"]["name"].get<std::string>();
    }

    static int getExecutionCount() { return s_totalExecutions.load(); }
    static void resetCounter() { s_totalExecutions = 0; }

private:
    std::string m_name;
    static std::atomic<int> s_totalExecutions;
};

std::atomic<int> CountingNode::s_totalExecutions{0};

class SumNode : public INode {
public:
    SumNode(int numInputs = 2) : m_numInputs(numInputs) {}

    void execute(Context& ctx) override {
        int sum = 0;
        for (int i = 0; i < m_numInputs; ++i) {
            sum += std::any_cast<int>(getInputValue(ctx, i));
        }
        setOutputValue(ctx, 0, sum);
    }

    ComponentMetadata getMetadata() const override {
        std::vector<PortInfo> inputs;
        for (int i = 0; i < m_numInputs; ++i) {
            inputs.push_back({"in" + std::to_string(i), typeid(int), true});
        }
        return {"SumNode", inputs, {{"out", typeid(int)}}, true, false};
    }

    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "numInputs") m_numInputs = std::any_cast<int>(value);
    }
    std::any getParameter(const std::string& name) const override {
        if (name == "numInputs") return m_numInputs;
        return {};
    }
    void serialize(nlohmann::json& j) const override {
        j["type"] = "SumNode";
        j["params"]["numInputs"] = m_numInputs;
    }
    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params") && j["params"].contains("numInputs"))
            m_numInputs = j["params"]["numInputs"].get<int>();
    }

private:
    int m_numInputs;
};

class IntConsumerNode : public INode {
public:
    void execute(Context& ctx) override {
        int val = std::any_cast<int>(getInputValue(ctx, 0));
        setOutputValue(ctx, 0, val);
        static std::mutex coutMutex;
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "[IntConsumer] Received: " << val << std::endl;
    }

    ComponentMetadata getMetadata() const override {
        return {"IntConsumer", {{"in", typeid(int), true}}, {{"out", typeid(int)}}, true, false};
    }

    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "IntConsumer"; }
    void deserialize(const nlohmann::json&) override {}
};

void registerAdditionalNodes() {
    auto& factory = NodeFactory::instance();
    factory.registerNode("CountingNode", []() { return std::make_unique<CountingNode>(""); });
    factory.registerNode("SumNode", []() { return std::make_unique<SumNode>(); });
    factory.registerNode("IntConsumer", []() { return std::make_unique<IntConsumerNode>(); });
}

std::string formatDuration(std::chrono::milliseconds ms) {
    std::ostringstream oss;
    oss << ms.count() << " ms";
    return oss.str();
}

// ========== Вспомогательные функции для рендера ==========

void createTestMeshFile(const std::string& path) {
    std::vector<float> vertices = {
        // Передняя грань
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        // Задняя грань
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        // Левая грань
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        // Правая грань
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
        // Верхняя грань
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
        // Нижняя грань
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f
    };
    std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0,       // передняя
        4, 5, 6, 6, 7, 4,       // задняя
        8, 9,10,10,11, 8,       // левая
       12,13,14,14,15,12,       // правая
       16,17,18,18,19,16,       // верхняя
       20,21,22,22,23,20        // нижняя
    };
    std::vector<int> attribSizes = {3, 3};

    std::ofstream file(path, std::ios::binary);
    uint32_t vcount = static_cast<uint32_t>(vertices.size());
    file.write(reinterpret_cast<const char*>(&vcount), sizeof(vcount));
    file.write(reinterpret_cast<const char*>(vertices.data()), vcount * sizeof(float));
    uint32_t icount = static_cast<uint32_t>(indices.size());
    file.write(reinterpret_cast<const char*>(&icount), sizeof(icount));
    file.write(reinterpret_cast<const char*>(indices.data()), icount * sizeof(uint32_t));
    uint32_t acount = static_cast<uint32_t>(attribSizes.size());
    file.write(reinterpret_cast<const char*>(&acount), sizeof(acount));
    file.write(reinterpret_cast<const char*>(attribSizes.data()), acount * sizeof(int));
}

static std::array<float, 16> identityMatrix() {
    std::array<float, 16> m{};
    m[0] = m[5] = m[10] = m[15] = 1.0f;
    return m;
}

static std::array<float, 16> rotateY(float angle) {
    std::array<float, 16> m = identityMatrix();
    float c = std::cos(angle);
    float s = std::sin(angle);
    m[0] = c;
    m[2] = s;
    m[8] = -s;
    m[10] = c;
    return m;
}

// ========== Главная функция ==========

int main() {
    try {
        // Инициализация общих компонентов
        initBasicTypes();
        registerBasicNodes();
        registerRenderNodes();
        registerAdditionalNodes();

        // ========== Тесты графа (консоль) ==========
        {
            std::cout << "=== Complex Graph Test (Dependencies + Caching) ===" << std::endl;
            CountingNode::resetCounter();

            Graph graph;
            const int numBranches = 3;

            std::vector<NodeId> constIds;
            for (int i = 0; i < numBranches; ++i) {
                auto constInt = std::make_unique<ConstantNode<int>>(i + 1);
                NodeId id = graph.addNode(std::move(constInt));
                constIds.push_back(id);
            }

            std::vector<NodeId> countIds;
            for (int i = 0; i < numBranches; ++i) {
                auto counter = std::make_unique<CountingNode>("branch" + std::to_string(i));
                NodeId id = graph.addNode(std::move(counter));
                countIds.push_back(id);
                graph.addConnection({constIds[i], 0, id, 0});
            }

            auto sumNode = std::make_unique<SumNode>(numBranches);
            NodeId sumId = graph.addNode(std::move(sumNode));
            for (int i = 0; i < numBranches; ++i) {
                graph.addConnection({countIds[i], 0, sumId, i});
            }

            auto finalCounter = std::make_unique<CountingNode>("final");
            NodeId finalId = graph.addNode(std::move(finalCounter));
            graph.addConnection({sumId, 0, finalId, 0});

            auto consumer = std::make_unique<FloatConsumerNode>();
            NodeId consumerId = graph.addNode(std::move(consumer));
            graph.addConnection({finalId, 0, consumerId, 0});

            std::cout << "Graph built with " << graph.getNodes().size() << " nodes." << std::endl;

            Executor executor(1);
            Context ctx;

            auto start = std::chrono::high_resolution_clock::now();
            executor.execute(graph, ctx, {consumerId});
            auto end = std::chrono::high_resolution_clock::now();
            std::cout << "Execution time: " << formatDuration(std::chrono::duration_cast<std::chrono::milliseconds>(end - start)) << std::endl;
            std::cout << "CountingNode total executions: " << CountingNode::getExecutionCount() << " (expected 4)" << std::endl;

            // Второе выполнение (кэш)
            start = std::chrono::high_resolution_clock::now();
            executor.execute(graph, ctx, {consumerId});
            end = std::chrono::high_resolution_clock::now();
            std::cout << "Execution time (cached): " << formatDuration(std::chrono::duration_cast<std::chrono::milliseconds>(end - start)) << std::endl;
            std::cout << "CountingNode total executions: " << CountingNode::getExecutionCount() << " (should be same)" << std::endl;

            // Инвалидация изменением константы
            INode* constNode = graph.getNode(constIds[1]);
            constNode->setParameter("value", 100);
            graph.invalidateSubgraph(constIds[1]);

            start = std::chrono::high_resolution_clock::now();
            executor.execute(graph, ctx, {consumerId});
            end = std::chrono::high_resolution_clock::now();
            std::cout << "Execution time (after change): " << formatDuration(std::chrono::duration_cast<std::chrono::milliseconds>(end - start)) << std::endl;
            std::cout << "CountingNode total executions: " << CountingNode::getExecutionCount() << " (should increase)" << std::endl;

            if (ctx.output.type() == typeid(float)) {
                float result = std::any_cast<float>(ctx.output);
                std::cout << "Final result: " << result << " (expected 416)" << std::endl;
            }
        }

        {
            std::cout << "\n=== Stress Test: Large Graph (Multi-threaded) ===" << std::endl;
            CountingNode::resetCounter();

            Graph bigGraph;
            const int numBigBranches = 50;
            std::vector<NodeId> bigRoots;

            for (int i = 0; i < numBigBranches; ++i) {
                auto constInt = std::make_unique<ConstantNode<int>>(i);
                NodeId cid = bigGraph.addNode(std::move(constInt));
                auto counter = std::make_unique<CountingNode>("stress" + std::to_string(i));
                NodeId cntId = bigGraph.addNode(std::move(counter));
                auto intConsumer = std::make_unique<IntConsumerNode>();
                NodeId fid = bigGraph.addNode(std::move(intConsumer));

                bigGraph.addConnection({cid, 0, cntId, 0});
                bigGraph.addConnection({cntId, 0, fid, 0});
                bigRoots.push_back(fid);
            }

            std::cout << "Big graph has " << bigGraph.getNodes().size() << " nodes." << std::endl;

            Executor bigExecutor(8);
            Context bigCtx;
            int prevCount = CountingNode::getExecutionCount();
            auto startStress = std::chrono::high_resolution_clock::now();
            bigExecutor.execute(bigGraph, bigCtx, bigRoots);
            auto endStress = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endStress - startStress);
            std::cout << "Execution time: " << formatDuration(elapsed) << std::endl;
            int newExecutions = CountingNode::getExecutionCount() - prevCount;
            std::cout << "CountingNode executions: " << newExecutions << " (expected " << numBigBranches << ")" << std::endl;
        }

        {
            std::cout << "\n=== Serialization Test ===" << std::endl;
            Graph graph;
            auto constInt = std::make_unique<ConstantNode<int>>(42);
            NodeId constId = graph.addNode(std::move(constInt));
            auto consumer = std::make_unique<FloatConsumerNode>();
            NodeId consumerId = graph.addNode(std::move(consumer));
            graph.addConnection({constId, 0, consumerId, 0});

            nlohmann::json j;
            graph.serialize(j);
            std::ofstream("test_graph.json") << j.dump(2);
            std::cout << "Graph saved to test_graph.json" << std::endl;

            Graph loadedGraph;
            loadedGraph.deserialize(j);
            std::cout << "Graph loaded, node count: " << loadedGraph.getNodes().size() << std::endl;

            NodeId loadedConsumerId = 0;
            for (const auto& nodePtr : loadedGraph.getNodes()) {
                if (nodePtr->getMetadata().name == "FloatConsumer") {
                    loadedConsumerId = nodePtr->getId();
                    break;
                }
            }

            if (loadedConsumerId != 0) {
                Executor loadExecutor(1);
                Context loadCtx;
                loadExecutor.execute(loadedGraph, loadCtx, {loadedConsumerId});
                if (loadCtx.output.type() == typeid(float)) {
                    float loadedResult = std::any_cast<float>(loadCtx.output);
                    std::cout << "Loaded graph output: " << loadedResult << " (expected 42)" << std::endl;
                }
            } else {
                std::cout << "FloatConsumer not found in loaded graph!" << std::endl;
            }
        }

        std::cout << "\nAll graph tests completed successfully!" << std::endl;
        std::cout << "\nPress Enter to start rendering test..." << std::endl;
        std::cin.get();

        // ========== Рендер тест (окно с вращающимся кубом) ==========
        {
            if (!glfwInit()) {
                std::cerr << "Failed to initialize GLFW" << std::endl;
                return -1;
            }
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            GLFWwindow* window = glfwCreateWindow(800, 600, "3Tone Render Test", nullptr, nullptr);
            if (!window) {
                std::cerr << "Failed to create window" << std::endl;
                glfwTerminate();
                return -1;
            }
            glfwMakeContextCurrent(window);
            glfwSwapInterval(1);

            auto& device = render::GraphicsDevice::instance();
            device.initialize(window);

            createTestMeshFile("cube.3tm");

            auto scene = std::make_shared<render::Scene>();

            std::ifstream file("cube.3tm", std::ios::binary);
            if (!file) throw std::runtime_error("Failed to open cube.3tm");
            uint32_t vcount;
            file.read(reinterpret_cast<char*>(&vcount), sizeof(vcount));
            std::vector<float> vertices(vcount);
            file.read(reinterpret_cast<char*>(vertices.data()), vcount * sizeof(float));
            uint32_t icount;
            file.read(reinterpret_cast<char*>(&icount), sizeof(icount));
            std::vector<uint32_t> indices(icount);
            file.read(reinterpret_cast<char*>(indices.data()), icount * sizeof(uint32_t));
            uint32_t acount;
            file.read(reinterpret_cast<char*>(&acount), sizeof(acount));
            std::vector<int> attribSizes(acount);
            file.read(reinterpret_cast<char*>(attribSizes.data()), acount * sizeof(int));
            auto mesh = device.createMesh(vertices, indices, attribSizes);

            const char* vertexSrc = R"(
                #version 450 core
                layout(location = 0) in vec3 aPos;
                uniform mat4 uModel;
                uniform mat4 uView;
                uniform mat4 uProjection;
                void main() {
                    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
                }
            )";
            const char* fragmentSrc = R"(
                #version 450 core
                uniform vec3 uColor;
                out vec4 FragColor;
                void main() {
                    FragColor = vec4(uColor, 1.0);
                }
            )";

            auto shader = device.createShader(vertexSrc, fragmentSrc);
            auto mat = device.createMaterial(shader);
            mat->setParameter("uColor", std::array<float,3>{1.0f, 0.5f, 0.2f}); // оранжевый

            scene->addRenderable(mesh, mat, identityMatrix());

            auto cam = std::make_shared<render::Camera>();
            cam->lookAt({0.0f, 1.0f, 3.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
            const float pi = 3.1415926535f;
            cam->setPerspective(45.0f * pi / 180.0f, 800.0f/600.0f, 0.1f, 100.0f);

            float angle = 0.0f;
            auto lastTime = std::chrono::high_resolution_clock::now();

            while (!glfwWindowShouldClose(window)) {
                glfwPollEvents();

                auto currentTime = std::chrono::high_resolution_clock::now();
                float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
                lastTime = currentTime;

                angle += deltaTime * 1.0f;
                if (angle > 2.0f * pi) angle -= 2.0f * pi;

                auto newTransform = rotateY(angle);
                scene = std::make_shared<render::Scene>();
                scene->addRenderable(mesh, mat, newTransform);

                device.clear(0.2f, 0.3f, 0.3f, 1.0f);
                device.setViewport(0, 0, 800, 600);

                render::CommandBuffer cmd;
                cmd.begin();
                cmd.setRenderTarget(nullptr);
                cmd.clear(0.2f, 0.3f, 0.3f, 1.0f);
                cmd.setViewport(0, 0, 800, 600);
                cmd.drawScene(*scene, *cam);
                cmd.end();
                cmd.execute();

                device.swapBuffers();
            }

            device.shutdown();
            glfwDestroyWindow(window);
            glfwTerminate();

            std::cout << "Rendering test completed." << std::endl;
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}