// src/nodes/text_texture_node.hpp
#pragma once
#include "../core/node.hpp"
#include "../assets/asset_manager.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace arxglue {

class TextTextureNode : public INode {
public:
    TextTextureNode(int width = 256, int height = 128, const std::string& text = "", 
                    uint8_t r = 255, uint8_t g = 255, uint8_t b = 255);
    
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

private:
    int m_width;
    int m_height;
    std::string m_text;
    uint8_t m_textR, m_textG, m_textB;
    std::shared_ptr<TextureAsset> m_cachedTexture;

    void generateTexture();
};

} // namespace arxglue