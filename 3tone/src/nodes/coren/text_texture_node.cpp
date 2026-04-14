#include "text_texture_node.hpp"
#include "../node_factory.hpp"
#include <cstring>
#include <algorithm>

namespace arxglue {

// Растровый шрифт 8x16
static const uint8_t font8x16[11][16] = {
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // '0'
    {0x18,0x38,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x00}, // '1'
    {0x3C,0x66,0x66,0x06,0x0C,0x18,0x30,0x60,0xC0,0xC0,0xC0,0x60,0x30,0x18,0x7E,0x00}, // '2'
    {0x3C,0x66,0x66,0x06,0x0C,0x1C,0x06,0x06,0x06,0x06,0x66,0x66,0x66,0x66,0x3C,0x00}, // '3'
    {0x0C,0x1C,0x3C,0x6C,0xCC,0xCC,0xCC,0xFE,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // '4'
    {0x7E,0x60,0x60,0x60,0x7C,0x66,0x06,0x06,0x06,0x06,0x66,0x66,0x66,0x66,0x3C,0x00}, // '5'
    {0x3C,0x66,0x66,0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // '6'
    {0x7E,0x66,0x06,0x0C,0x0C,0x18,0x18,0x30,0x30,0x60,0x60,0x60,0x60,0x60,0x60,0x00}, // '7'
    {0x3C,0x66,0x66,0x66,0x66,0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // '8'
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x3E,0x06,0x06,0x06,0x66,0x66,0x66,0x3C,0x00}, // '9'
    {0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00}  // ':'
};

void TextTextureNode::generateTexture() {
    int scale = 4; // увеличение в 4 раза
    int charW = 8 * scale;
    int charH = 16 * scale;
    int padding = 4 * scale;
    int textLen = (int)m_text.length();
    
    // Реальная ширина текста в пикселях
    int textWidth = textLen * (charW + padding) - padding;
    int texW = textWidth + padding * 2;
    if (texW < m_width) texW = m_width;
    int texH = charH + padding * 2;
    if (texH < m_height) texH = m_height;
    
    // Прозрачный фон (альфа = 0)
    std::vector<uint8_t> pixels(texW * texH * 4, 0);
    // Все пиксели уже 0, альфа=0, фон прозрачный
    
    // Центрируем текст по горизонтали
    int xOffset = (texW - textWidth) / 2;
    int yOffset = padding;
    
    for (size_t i = 0; i < m_text.size(); ++i) {
        char c = m_text[i];
        int fontIdx = -1;
        if (c >= '0' && c <= '9') fontIdx = c - '0';
        else if (c == ':') fontIdx = 10;
        if (fontIdx == -1) continue;
        
        for (int row = 0; row < 16; ++row) {
            uint8_t bits = font8x16[fontIdx][row];
            for (int col = 0; col < 8; ++col) {
                if (bits & (1 << (7 - col))) {
                    // Рисуем увеличенный пиксель
                    for (int sy = 0; sy < scale; ++sy) {
                        for (int sx = 0; sx < scale; ++sx) {
                            // Отражение по вертикали (OpenGL Y снизу вверх)
                            int px = xOffset + col * scale + sx;
                            int py = texH - 1 - (yOffset + row * scale + sy);
                            if (px >= 0 && px < texW && py >= 0 && py < texH) {
                                size_t idx = (py * texW + px) * 4;
                                pixels[idx] = 255;   // R
                                pixels[idx+1] = 255; // G
                                pixels[idx+2] = 255; // B
                                pixels[idx+3] = 255; // A (непрозрачный текст)
                            }
                        }
                    }
                }
            }
        }
        xOffset += charW + padding;
    }
    
    m_cachedTexture = std::make_shared<TextureAsset>(texW, texH, pixels);
}

TextTextureNode::TextTextureNode(int width, int height, const std::string& text,
                                 uint8_t r, uint8_t g, uint8_t b)
    : m_width(width), m_height(height), m_text(text), m_textR(r), m_textG(g), m_textB(b) {
    generateTexture();
}

void TextTextureNode::execute(Context& ctx) {
    if (!m_cachedTexture) generateTexture();
    setOutputValue(ctx, 0, m_cachedTexture);
}

ComponentMetadata TextTextureNode::getMetadata() const {
    return {"TextTexture", {}, {{"texture", typeid(std::shared_ptr<TextureAsset>)}}, true, false};
}

void TextTextureNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "text") {
        m_text = std::any_cast<std::string>(value);
        generateTexture();
    } else if (name == "width") {
        m_width = std::any_cast<int>(value);
        generateTexture();
    } else if (name == "height") {
        m_height = std::any_cast<int>(value);
        generateTexture();
    } else if (name == "colorR") {
        m_textR = std::any_cast<int>(value);
        generateTexture();
    } else if (name == "colorG") {
        m_textG = std::any_cast<int>(value);
        generateTexture();
    } else if (name == "colorB") {
        m_textB = std::any_cast<int>(value);
        generateTexture();
    }
}

std::any TextTextureNode::getParameter(const std::string& name) const {
    if (name == "text") return m_text;
    if (name == "width") return m_width;
    if (name == "height") return m_height;
    if (name == "colorR") return (int)m_textR;
    if (name == "colorG") return (int)m_textG;
    if (name == "colorB") return (int)m_textB;
    return {};
}

void TextTextureNode::serialize(nlohmann::json& j) const {
    j["type"] = "TextTexture";
    j["params"]["text"] = m_text;
    j["params"]["width"] = m_width;
    j["params"]["height"] = m_height;
    j["params"]["colorR"] = m_textR;
    j["params"]["colorG"] = m_textG;
    j["params"]["colorB"] = m_textB;
}

void TextTextureNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        auto& p = j["params"];
        if (p.contains("text")) m_text = p["text"].get<std::string>();
        if (p.contains("width")) m_width = p["width"].get<int>();
        if (p.contains("height")) m_height = p["height"].get<int>();
        if (p.contains("colorR")) m_textR = p["colorR"].get<int>();
        if (p.contains("colorG")) m_textG = p["colorG"].get<int>();
        if (p.contains("colorB")) m_textB = p["colorB"].get<int>();
        generateTexture();
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