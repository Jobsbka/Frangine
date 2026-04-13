#include "graph.hpp"
#include "executor.hpp"
#include "../types/type_system.hpp"
#include "../nodes/node_factory.hpp"
#include "../nodes/basic_nodes.hpp"
#include "../nodes/convert_node.hpp"
#include "../nodes/asset_node.hpp"
#include "../nodes/checker_texture.hpp"
#include "../nodes/text_texture_node.hpp"

namespace arxglue::core {

void initializeModule() {
    // Инициализация базовых типов
    initBasicTypes();
    // Регистрация базовых узлов
    registerBasicNodes();

    auto& factory = NodeFactory::instance();
    // Дополнительные узлы, которые могут быть не включены в registerBasicNodes
    factory.registerNode("ConvertNode", []() {
        return std::make_unique<ConvertNode>();
    });
    factory.registerNode("AssetNode", []() {
        return std::make_unique<AssetNode>();
    });
    factory.registerNode("CheckerTexture", []() {
        return std::make_unique<CheckerTextureNode>();
    });
    factory.registerNode("TextTexture", []() {
        return std::make_unique<TextTextureNode>();
    });
}

} // namespace arxglue::core