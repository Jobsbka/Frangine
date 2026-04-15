// src/nodes/coren/text_texture_node.cpp
#include "text_texture_node.hpp"
#include "../node_factory.hpp"
#include "../../render/font.hpp"
#include <iostream>

namespace arxglue {

TextTextureNode::TextTextureNode() = default;

void TextTextureNode::execute(Context& ctx) {
    if (!m_cachedTexture || m_text != m_lastText) {
        try {
            ui::Font font(m_fontPath, m_fontSize);
            m_cachedTexture = font.renderText(m_text, m_color);
            m_lastText = m_text;
        } catch (const std::exception& e) {
            std::cerr << "[TextTextureNode] Font error: " << e.what() << std::endl;
            std::vector<uint8_t> dummy(64 * 64 * 4, 0);
            m_cachedTexture = std::make_shared<TextureAsset>(64, 64, dummy);
        }
    }
    setOutputValue(ctx, 0, m_cachedTexture);
}

ComponentMetadata TextTextureNode::getMetadata() const {
    return {"TextTexture", {}, {{"texture", typeid(std::shared_ptr<TextureAsset>)}}, true, false};
}

void TextTextureNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "text") {
        m_text = std::any_cast<std::string>(value);
    } else if (name == "fontPath") {
        m_fontPath = std::any_cast<std::string>(value);
    } else if (name == "fontSize") {
        m_fontSize = std::any_cast<float>(value);
    } else if (name == "color") {
        m_color = std::any_cast<uint32_t>(value);
    }
}

std::any TextTextureNode::getParameter(const std::string& name) const {
    if (name == "text") return m_text;
    if (name == "fontPath") return m_fontPath;
    if (name == "fontSize") return m_fontSize;
    if (name == "color") return m_color;
    return {};
}

void TextTextureNode::serialize(nlohmann::json& j) const {
    j["type"] = "TextTexture";
    j["params"]["text"] = m_text;
    j["params"]["fontPath"] = m_fontPath;
    j["params"]["fontSize"] = m_fontSize;
    j["params"]["color"] = m_color;
}

void TextTextureNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        auto& p = j["params"];
        if (p.contains("text")) m_text = p["text"].get<std::string>();
        if (p.contains("fontPath")) m_fontPath = p["fontPath"].get<std::string>();
        if (p.contains("fontSize")) m_fontSize = p["fontSize"].get<float>();
        if (p.contains("color")) m_color = p["color"].get<uint32_t>();
    }
}

static bool registerTextTextureNode() {
    NodeFactory::instance().registerNode("TextTexture", [](){
        return std::make_unique<TextTextureNode>();
    });
    return true;
}
static bool dummyTextTexture = registerTextTextureNode();

} // namespace arxglue