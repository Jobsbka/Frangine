// src/nodes/basic_nodes.cpp
#include "basic_nodes.hpp"
#include "../node_factory.hpp"
#include "../../assets/asset_manager.hpp"
#include "../../nodes/coren/checker_texture.hpp"
#include "../../nodes/coren/asset_node.hpp"
#include "../../nodes/coren/convert_node.hpp"
#include "../../nodes/coren/text_texture_node.hpp"
#include <random> 

namespace arxglue {

void AddNode::execute(Context& ctx) {
    int a = std::any_cast<int>(getInputValue(ctx, 0));
    int b = std::any_cast<int>(getInputValue(ctx, 1));
    setOutputValue(ctx, 0, a + b);
}

void LoadTextureNode::execute(Context& ctx) {
    auto tex = AssetManager::instance().loadTexture(m_path);
    if (!tex) {
        std::vector<uint8_t> dummy(64 * 64 * 4, 128);
        tex = std::make_shared<TextureAsset>(64, 64, dummy);
    }
    setOutputValue(ctx, 0, tex);
}

void PerlinNoiseNode::execute(Context& ctx) {
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float value = dist(rng);
    setOutputValue(ctx, 0, value);
}

void registerBasicNodes() {
    auto& factory = NodeFactory::instance();
    factory.registerNode("ConstantInt", []() { return std::make_unique<ConstantNode<int>>(0); });
    factory.registerNode("ConstantFloat", []() { return std::make_unique<ConstantNode<float>>(0.0f); });
    factory.registerNode("Add", []() { return std::make_unique<AddNode>(); });
    factory.registerNode("LoadTexture", []() { return std::make_unique<LoadTextureNode>(); });
    factory.registerNode("PerlinNoise", []() { return std::make_unique<PerlinNoiseNode>(); });
    factory.registerNode("CheckerTexture", []() { return std::make_unique<CheckerTextureNode>(); });
    factory.registerNode("AssetNode", []() { return std::make_unique<AssetNode>(); });
    factory.registerNode("ConvertNode", []() { return std::make_unique<ConvertNode>(); });
    factory.registerNode("FloatConsumer", []() { return std::make_unique<FloatConsumerNode>(); });
    factory.registerNode("SlowNode", []() { return std::make_unique<SlowNode>(); });
    factory.registerNode("TextTexture", []() { return std::make_unique<TextTextureNode>(); });
    // Узел YToColorNode зарегистрирован в Game.cpp через статическую переменную,
    // поэтому здесь его не регистрируем.
}

} // namespace arxglue