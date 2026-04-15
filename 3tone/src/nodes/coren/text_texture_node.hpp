// src/nodes/coren/text_texture_node.hpp
#pragma once
#include "../../core/node.hpp"
#include "../../assets/asset_manager.hpp"
#include <string>
#include <memory>

namespace arxglue {

class TextTextureNode : public INode {
public:
    TextTextureNode();
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

private:
    std::string m_text;
    std::string m_fontPath = "include/fonts/calibri.ttf";
    float m_fontSize = 24.0f;
    uint32_t m_color = 0xFFFFFFFF;
    std::shared_ptr<TextureAsset> m_cachedTexture;
    std::string m_lastText;
};

} // namespace arxglue