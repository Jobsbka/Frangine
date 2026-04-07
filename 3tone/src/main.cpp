#include "core/graph.hpp"
#include "core/executor.hpp"
#include "core/baking.hpp"
#include "nodes/basic_nodes.hpp"
#include "nodes/checker_texture.hpp"
#include "nodes/asset_node.hpp"
#include "types/type_system.hpp"
#include "assets/asset_manager.hpp"
#include <iostream>
#include <exception>
#include <chrono>
#include <filesystem>

using namespace arxglue;

int main() {
    try {
        initBasicTypes();
        registerBasicNodes();

        std::cout << "=== 3Tone Baking Demo ===" << std::endl;

        // Создаём граф: CheckerTexture -> (результат)
        Graph graph;

        auto checker = std::make_unique<CheckerTextureNode>(512, 512, 8, 8);
        NodeId checkerId = graph.addNode(std::move(checker));

        Executor executor(2);
        Context ctx;

        // Первое выполнение – генерация текстуры
        std::cout << "Generating checker texture..." << std::endl;
        executor.execute(graph, ctx, {checkerId});

        if (!ctx.output.has_value()) {
            std::cerr << "No output from checker texture!" << std::endl;
            return 1;
        }
        std::cout << "Output type: " << ctx.output.type().name() << std::endl;
        auto tex = std::any_cast<std::shared_ptr<TextureAsset>>(ctx.output);
        std::cout << "Generated texture size: " << tex->width << "x" << tex->height << std::endl;

        // Запекаем подграф в PNG
        std::string bakePath = "baked_checker.png";
        std::cout << "Baking subgraph to " << bakePath << "..." << std::endl;
        NodeId assetId = bakeSubgraph(graph, checkerId, bakePath, "texture");
        std::cout << "Baked. New AssetNode ID: " << assetId << std::endl;

        // Выполняем граф снова, теперь через AssetNode
        Context ctx2;
        executor.execute(graph, ctx2, {assetId});

        std::cout << "ctx2.output type: " << ctx2.output.type().name() << std::endl;
        if (ctx2.output.type() == typeid(std::shared_ptr<TextureAsset>)) {
            auto tex2 = std::any_cast<std::shared_ptr<TextureAsset>>(ctx2.output);
            std::cout << "AssetNode texture size: " << tex2->width << "x" << tex2->height << std::endl;

            // Сравниваем пиксели (первые несколько)
            bool match = (tex->width == tex2->width && tex->height == tex2->height);
            if (match) {
                for (size_t i = 0; i < std::min(tex->pixels.size(), tex2->pixels.size()); ++i) {
                    if (tex->pixels[i] != tex2->pixels[i]) {
                        match = false;
                        break;
                    }
                }
            }
            std::cout << "Pixel comparison: " << (match ? "MATCH" : "DIFFER") << std::endl;
        } else {
            std::cerr << "Unexpected output type from AssetNode" << std::endl;
            if (ctx2.output.has_value()) {
                std::cerr << "Actual type: " << ctx2.output.type().name() << std::endl;
            } else {
                std::cerr << "Output is empty!" << std::endl;
            }
        }

        // Сериализация графа с запечённым узлом
        nlohmann::json j;
        graph.serialize(j);
        std::ofstream("baked_graph.json") << j.dump(2);
        std::cout << "Graph saved to baked_graph.json" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n=== Type Conversion Test ===" << std::endl;
    Graph graph2;

    // Узел, выдающий целое число 42
    auto constInt = std::make_unique<ConstantNode<int>>(42);
    NodeId constId = graph2.addNode(std::move(constInt));

    // Узел-потребитель, ожидающий float
    auto consumer = std::make_unique<FloatConsumerNode>();
    NodeId consumerId = graph2.addNode(std::move(consumer));

    // Соединяем int -> float (автоматически вставится ConvertNode)
    Connection conn;
    conn.srcNode = constId;
    conn.srcPort = 0;
    conn.dstNode = consumerId;
    conn.dstPort = 0;

    try {
        graph2.addConnection(conn);
        std::cout << "Connection added successfully (converter inserted automatically)." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to add connection: " << e.what() << std::endl;
        return 1;
    }

    // Проверяем, что в графе теперь три узла
    std::cout << "Graph2 node count: " << graph2.getNodes().size() << std::endl;
    for (const auto& nodePtr : graph2.getNodes()) {
        std::cout << "  Node: " << nodePtr->getMetadata().name << std::endl;
    }

    // Выполняем граф
    Executor exec2(1);
    Context ctx3;
    exec2.execute(graph2, ctx3, {consumerId});

    // Проверяем результат (должен быть float 42.0f)
    if (ctx3.output.has_value() && ctx3.output.type() == typeid(float)) {
        float result = std::any_cast<float>(ctx3.output);
        std::cout << "Final output: " << result << std::endl;
    } else {
        std::cerr << "Unexpected output type!" << std::endl;
    }

    std::cout << "\nPress Enter to exit..." << std::endl;
    std::cin.get();
    return 0;
}