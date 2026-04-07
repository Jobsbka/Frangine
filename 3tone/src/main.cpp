#include "core/graph.hpp"
#include "core/executor.hpp"
#include "nodes/basic_nodes.hpp"
#include "types/type_system.hpp"
#include <iostream>
#include <exception>

using namespace arxglue;

int main() {
    try {
        initBasicTypes();
        registerBasicNodes();

        Graph graph;

        auto const10 = std::make_unique<ConstantNode<int>>(10);
        auto const20 = std::make_unique<ConstantNode<int>>(20);
        auto addNode = std::make_unique<AddNode>();

        NodeId id10 = graph.addNode(std::move(const10));
        NodeId id20 = graph.addNode(std::move(const20));
        NodeId idAdd = graph.addNode(std::move(addNode));

        graph.addConnection({id10, 0, idAdd, 0});
        graph.addConnection({id20, 0, idAdd, 1});

        Executor executor(2);
        Context ctx;

        std::cout << "Starting first execution..." << std::endl;
        executor.execute(graph, ctx, {idAdd});
        int result = std::any_cast<int>(ctx.output);
        std::cout << "Result: " << result << std::endl;

        nlohmann::json j;
        graph.serialize(j);
        std::cout << "Serialized graph:\n" << j.dump(2) << std::endl;

        Graph graph2;
        graph2.deserialize(j);
        Executor executor2;
        Context ctx2;
        executor2.execute(graph2, ctx2, {graph2.getNodes().back()->getId()});
        int result2 = std::any_cast<int>(ctx2.output);
        std::cout << "Deserialized result: " << result2 << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << std::endl;
        std::cin.get();
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception!" << std::endl;
        std::cin.get();
        return 1;
    }

    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    return 0;
}