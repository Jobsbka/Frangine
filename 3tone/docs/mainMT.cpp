// src/main.cpp
#include "core/graph.hpp"
#include "core/executor.hpp"
#include "nodes/basic_nodes.hpp"
#include "nodes/node_factory.hpp"
#include "types/type_system.hpp"
#include <iostream>
#include <chrono>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <thread>

using namespace arxglue;

// ========== Тестовые узлы ==========

class DelayNode : public INode {
public:
    explicit DelayNode(int ms = 10) : m_delayMs(ms) {}

    void execute(Context& ctx) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_delayMs));
        int val = std::any_cast<int>(getInputValue(ctx, 0));
        setOutputValue(ctx, 0, val);
    }

    ComponentMetadata getMetadata() const override {
        return {"DelayNode", {{"in", typeid(int), true}}, {{"out", typeid(int)}}, true, false};
    }

    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "delay") m_delayMs = std::any_cast<int>(value);
    }

    std::any getParameter(const std::string& name) const override {
        if (name == "delay") return m_delayMs;
        return {};
    }

    void serialize(nlohmann::json& j) const override {
        j["type"] = "DelayNode";
        j["params"]["delay"] = m_delayMs;
    }

    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params") && j["params"].contains("delay"))
            m_delayMs = j["params"]["delay"].get<int>();
    }

private:
    int m_delayMs;
};

class CountingNode : public INode {
public:
    CountingNode() { resetCounter(); }

    void execute(Context& ctx) override {
        ++s_totalExecutions;
        int val = std::any_cast<int>(getInputValue(ctx, 0));
        setOutputValue(ctx, 0, val * 2);
    }

    ComponentMetadata getMetadata() const override {
        return {"CountingNode", {{"in", typeid(int), true}}, {{"out", typeid(int)}}, true, false};
    }

    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "CountingNode"; }
    void deserialize(const nlohmann::json&) override {}

    static int getExecutionCount() { return s_totalExecutions.load(); }
    static void resetCounter() { s_totalExecutions = 0; }

private:
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
    }

    ComponentMetadata getMetadata() const override {
        return {"IntConsumer", {{"in", typeid(int), true}}, {{"out", typeid(int)}}, true, false};
    }

    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "IntConsumer"; }
    void deserialize(const nlohmann::json&) override {}
};

void registerTestNodes() {
    auto& factory = NodeFactory::instance();
    factory.registerNode("ConstantInt", []() { return std::make_unique<ConstantNode<int>>(0); });
    factory.registerNode("DelayNode", []() { return std::make_unique<DelayNode>(); });
    factory.registerNode("CountingNode", []() { return std::make_unique<CountingNode>(); });
    factory.registerNode("SumNode", []() { return std::make_unique<SumNode>(); });
    factory.registerNode("IntConsumer", []() { return std::make_unique<IntConsumerNode>(); });
}

std::string formatMs(std::chrono::milliseconds ms) {
    return std::to_string(ms.count()) + " ms";
}

// ========== Тесты ==========

void testParallelIndependentBranches() {
    std::cout << "\n=== Test 1: Parallel independent branches ===" << std::endl;
    
    const int numBranches = 16;
    const int delayMs = 20;
    Graph graph;
    std::vector<NodeId> roots;

    for (int i = 0; i < numBranches; ++i) {
        auto constNode = std::make_unique<ConstantNode<int>>(i);
        NodeId constId = graph.addNode(std::move(constNode));
        auto delayNode = std::make_unique<DelayNode>(delayMs);
        NodeId delayId = graph.addNode(std::move(delayNode));
        auto consumer = std::make_unique<IntConsumerNode>();
        NodeId consumerId = graph.addNode(std::move(consumer));
        
        graph.addConnection({constId, 0, delayId, 0});
        graph.addConnection({delayId, 0, consumerId, 0});
        roots.push_back(consumerId);
    }

    Executor executor(8);
    Context ctx;

    auto start = std::chrono::high_resolution_clock::now();
    executor.execute(graph, ctx, roots);
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Executed " << numBranches << " branches each with " << delayMs << " ms delay." << std::endl;
    std::cout << "Total time: " << formatMs(elapsed) << std::endl;
    std::cout << "Expected time if sequential: ~" << (numBranches * delayMs) << " ms" << std::endl;
    if (elapsed.count() > 0) {
        std::cout << "Parallel speedup: ~" << static_cast<float>(numBranches * delayMs) / elapsed.count() << "x" << std::endl;
    }
    if (elapsed.count() < delayMs || elapsed.count() > delayMs * 4) {
        std::cout << "WARNING: Unexpected execution time!" << std::endl;
    } else {
        std::cout << "Parallel execution confirmed." << std::endl;
    }
}

void testDependencyGraph() {
    std::cout << "\n=== Test 2: Dependency graph with caching ===" << std::endl;
    
    Graph graph;
    auto c1 = std::make_unique<ConstantNode<int>>(10);
    auto c2 = std::make_unique<ConstantNode<int>>(20);
    NodeId c1Id = graph.addNode(std::move(c1));
    NodeId c2Id = graph.addNode(std::move(c2));
    
    auto cnt1 = std::make_unique<CountingNode>();
    auto cnt2 = std::make_unique<CountingNode>();
    NodeId cnt1Id = graph.addNode(std::move(cnt1));
    NodeId cnt2Id = graph.addNode(std::move(cnt2));
    graph.addConnection({c1Id, 0, cnt1Id, 0});
    graph.addConnection({c2Id, 0, cnt2Id, 0});
    
    auto sum = std::make_unique<SumNode>(2);
    NodeId sumId = graph.addNode(std::move(sum));
    graph.addConnection({cnt1Id, 0, sumId, 0});
    graph.addConnection({cnt2Id, 0, sumId, 1});
    
    auto finalCnt = std::make_unique<CountingNode>();
    NodeId finalId = graph.addNode(std::move(finalCnt));
    graph.addConnection({sumId, 0, finalId, 0});
    
    auto consumer = std::make_unique<IntConsumerNode>();
    NodeId consumerId = graph.addNode(std::move(consumer));
    graph.addConnection({finalId, 0, consumerId, 0});

    Executor executor(4);
    Context ctx;
    CountingNode::resetCounter();

    executor.execute(graph, ctx, {consumerId});
    int expected = ((10 * 2) + (20 * 2)) * 2; // 120
    int result = std::any_cast<int>(ctx.output);
    std::cout << "First run result: " << result << " (expected " << expected << ")" << std::endl;
    std::cout << "CountingNode executions: " << CountingNode::getExecutionCount() << " (expected 3)" << std::endl;

    CountingNode::resetCounter();
    executor.execute(graph, ctx, {consumerId});
    result = std::any_cast<int>(ctx.output);
    std::cout << "Second run (cached) result: " << result << std::endl;
    std::cout << "CountingNode executions: " << CountingNode::getExecutionCount() << " (expected 0)" << std::endl;
}

void testInvalidation() {
    std::cout << "\n=== Test 3: Invalidation and partial recomputation ===" << std::endl;
    
    Graph graph;
    const int numBranches = 5;
    std::vector<NodeId> constIds;
    std::vector<NodeId> countIds;
    
    for (int i = 0; i < numBranches; ++i) {
        auto c = std::make_unique<ConstantNode<int>>(i + 1);
        NodeId cid = graph.addNode(std::move(c));
        constIds.push_back(cid);
        
        auto cnt = std::make_unique<CountingNode>();
        NodeId cntId = graph.addNode(std::move(cnt));
        countIds.push_back(cntId);
        graph.addConnection({cid, 0, cntId, 0});
    }
    
    auto sum = std::make_unique<SumNode>(numBranches);
    NodeId sumId = graph.addNode(std::move(sum));
    for (int i = 0; i < numBranches; ++i) {
        graph.addConnection({countIds[i], 0, sumId, i});
    }
    
    auto finalCnt = std::make_unique<CountingNode>();
    NodeId finalId = graph.addNode(std::move(finalCnt));
    graph.addConnection({sumId, 0, finalId, 0});
    
    auto consumer = std::make_unique<IntConsumerNode>();
    NodeId consumerId = graph.addNode(std::move(consumer));
    graph.addConnection({finalId, 0, consumerId, 0});

    Executor executor(4);
    Context ctx;
    CountingNode::resetCounter();

    executor.execute(graph, ctx, {consumerId});
    int expected = 60;
    int result = std::any_cast<int>(ctx.output);
    std::cout << "First run result: " << result << " (expected " << expected << ")" << std::endl;
    std::cout << "CountingNode executions: " << CountingNode::getExecutionCount() << " (expected 6)" << std::endl;

    CountingNode::resetCounter();
    INode* changedConst = graph.getNode(constIds[1]);
    changedConst->setParameter("value", 100);
    executor.invalidate(constIds[1]);
    
    executor.execute(graph, ctx, {consumerId});
    expected = 452;
    result = std::any_cast<int>(ctx.output);
    std::cout << "After invalidation result: " << result << " (expected " << expected << ")" << std::endl;
    int afterInvalidExecutions = CountingNode::getExecutionCount();
    std::cout << "CountingNode executions after invalidation: " << afterInvalidExecutions 
              << " (expected 2: changed branch and finalCnt)" << std::endl;
}

void testStressInvalidation() {
    std::cout << "\n=== Test 4: Stress test with multiple invalidations ===" << std::endl;
    
    const int numBranches = 30;
    const int numRounds = 5;
    Graph graph;
    std::vector<NodeId> constIds;
    std::vector<NodeId> countIds;
    
    std::vector<int> currentValues(numBranches);
    for (int i = 0; i < numBranches; ++i) {
        currentValues[i] = i;
        auto c = std::make_unique<ConstantNode<int>>(i);
        NodeId cid = graph.addNode(std::move(c));
        constIds.push_back(cid);
        
        auto cnt = std::make_unique<CountingNode>();
        NodeId cntId = graph.addNode(std::move(cnt));
        countIds.push_back(cntId);
        graph.addConnection({cid, 0, cntId, 0});
    }
    
    auto sum = std::make_unique<SumNode>(numBranches);
    NodeId sumId = graph.addNode(std::move(sum));
    for (int i = 0; i < numBranches; ++i) {
        graph.addConnection({countIds[i], 0, sumId, i});
    }
    
    auto finalCnt = std::make_unique<CountingNode>();
    NodeId finalId = graph.addNode(std::move(finalCnt));
    graph.addConnection({sumId, 0, finalId, 0});
    
    auto consumer = std::make_unique<IntConsumerNode>();
    NodeId consumerId = graph.addNode(std::move(consumer));
    graph.addConnection({finalId, 0, consumerId, 0});

    Executor executor(std::thread::hardware_concurrency());
    Context ctx;

    executor.execute(graph, ctx, {consumerId});
    int baseResult = std::any_cast<int>(ctx.output);
    std::cout << "Initial result: " << baseResult << std::endl;

    for (int round = 0; round < numRounds; ++round) {
        int changedIdx = (round * 7) % numBranches;
        int newValue = (round + 1) * 50;
        
        INode* node = graph.getNode(constIds[changedIdx]);
        int oldValue = currentValues[changedIdx];
        node->setParameter("value", newValue);
        currentValues[changedIdx] = newValue;
        executor.invalidate(constIds[changedIdx]);
        
        executor.execute(graph, ctx, {consumerId});
        int result = std::any_cast<int>(ctx.output);
        
        int expected = 0;
        for (int i = 0; i < numBranches; ++i) {
            expected += currentValues[i] * 2;
        }
        expected *= 2;
        
        std::cout << "Round " << round << ": changed node " << changedIdx 
                  << " from " << oldValue << " to " << newValue 
                  << " -> result " << result << " (expected " << expected << ")" << std::endl;
    }

    // Восстанавливаем исходные значения и проверяем консистентность
    for (int i = 0; i < numBranches; ++i) {
        INode* node = graph.getNode(constIds[i]);
        node->setParameter("value", i);
        currentValues[i] = i;
        executor.invalidate(constIds[i]);   // <-- ключевое исправление
    }
    executor.execute(graph, ctx, {consumerId});
    int finalCheck = std::any_cast<int>(ctx.output);
    if (finalCheck == baseResult) {
        std::cout << "Stress test passed: final consistency OK." << std::endl;
    } else {
        std::cout << "Stress test FAILED: final result " << finalCheck << " != " << baseResult << std::endl;
    }
}

int main() {
    try {
        initBasicTypes();
        registerBasicNodes();
        registerTestNodes();

        std::cout << "=== 3Tone Executor Test Suite ===" << std::endl;
        
        testParallelIndependentBranches();
        testDependencyGraph();
        testInvalidation();
        testStressInvalidation();

        std::cout << "\nAll executor tests completed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}