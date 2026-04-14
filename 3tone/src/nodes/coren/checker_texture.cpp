#include "checker_texture.hpp"
#include <vector>

namespace arxglue {

void CheckerTextureNode::execute(Context& ctx) {
    std::vector<uint8_t> pixels(m_width * m_height * 4); // RGBA

    int cellWidth = m_width / m_cellsX;
    int cellHeight = m_height / m_cellsY;
    if (cellWidth < 1) cellWidth = 1;
    if (cellHeight < 1) cellHeight = 1;

    for (int y = 0; y < m_height; ++y) {
        int cellY = y / cellHeight;
        for (int x = 0; x < m_width; ++x) {
            int cellX = x / cellWidth;
            bool isWhite = ((cellX + cellY) % 2) == 0;
            uint8_t color = isWhite ? 255 : 0;

            size_t idx = (y * m_width + x) * 4;
            pixels[idx + 0] = color; // R
            pixels[idx + 1] = color; // G
            pixels[idx + 2] = color; // B
            pixels[idx + 3] = 255;   // A
        }
    }

    auto texture = std::make_shared<TextureAsset>(m_width, m_height, pixels);
    setOutputValue(ctx, 0, texture);
}

} // namespace arxglue