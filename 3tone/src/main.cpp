#include "core/graph.hpp"
#include "core/executor.hpp"
#include "nodes/basic_nodes.hpp"
#include "types/type_system.hpp"
#include <iostream>
#include <exception>
#include <chrono>

using namespace arxglue;

int main() {
    try {
        initBasicTypes();
        registerBasicNodes();

        // Создаём граф: constInt(5) + constFloat(3.7f) -> Add (ожидает int, float сконвертируется)
        Graph graph;

        auto constInt = std::make_unique<ConstantNode<int>>(5);
        auto constFloat = std::make_unique<ConstantNode<float>>(3.7f);
        auto addNode = std::make_unique<AddNode>();

        NodeId idInt = graph.addNode(std::move(constInt));
        NodeId idFloat = graph.addNode(std::move(constFloat));
        NodeId idAdd = graph.addNode(std::move(addNode));

        graph.addConnection({idInt, 0, idAdd, 0});
        graph.addConnection({idFloat, 0, idAdd, 1});

        Executor executor(2);
        Context ctx;

        std::cout << "=== First execution (fresh) ===" << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        executor.execute(graph, ctx, {idAdd});
        auto end = std::chrono::high_resolution_clock::now();
        int result = std::any_cast<int>(ctx.output);
        std::cout << "Result: " << result << " (5 + 3.7 -> 5 + 3 = 8? Wait, conversion float->int truncates: 3.7 -> 3, so 5+3=8)" << std::endl;
        std::cout << "Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " µs" << std::endl;

        std::cout << "\n=== Second execution (cached, no changes) ===" << std::endl;
        start = std::chrono::high_resolution_clock::now();
        executor.execute(graph, ctx, {idAdd});
        end = std::chrono::high_resolution_clock::now();
        result = std::any_cast<int>(ctx.output);
        std::cout << "Result: " << result << std::endl;
        std::cout << "Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " µs (should be much faster due to caching)" << std::endl;

        // Сериализация и десериализация
        nlohmann::json j;
        graph.serialize(j);
        std::cout << "\nSerialized graph:\n" << j.dump(2) << std::endl;

        Graph graph2;
        graph2.deserialize(j);
        Executor executor2(1);
        Context ctx2;
        executor2.execute(graph2, ctx2, {graph2.getNodes().back()->getId()});
        int result2 = std::any_cast<int>(ctx2.output);
        std::cout << "Deserialized result: " << result2 << std::endl;

        // Дополнительно: тест с PerlinNoise (volatile) – не должен кэшироваться
        std::cout << "\n=== Volatile node test (PerlinNoise) ===" << std::endl;
        Graph graphNoise;
        auto noiseNode = std::make_unique<PerlinNoiseNode>();
        NodeId idNoise = graphNoise.addNode(std::move(noiseNode));
        Executor execNoise;
        Context ctxNoise;
        execNoise.execute(graphNoise, ctxNoise, {idNoise});
        float val1 = std::any_cast<float>(ctxNoise.output);
        execNoise.execute(graphNoise, ctxNoise, {idNoise});
        float val2 = std::any_cast<float>(ctxNoise.output);
        std::cout << "Noise values: " << val1 << ", " << val2 << " (should differ, caching disabled)" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << std::endl;
        std::cin.get();
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception!" << std::endl;
        std::cin.get();
        return 1;
    }

    std::cout << "\nPress Enter to exit..." << std::endl;
    std::cin.get();
    return 0;
}