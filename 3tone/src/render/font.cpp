// src/render/font.cpp
#include "font.hpp"
#include "../assets/asset_manager.hpp"
#include "stb_truetype.h"
#include <fstream>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <iostream>

namespace arxglue::ui {

struct Font::Impl {
    std::vector<uint8_t> fontBuffer;
    stbtt_bakedchar cdata[96]; // ASCII 32..126
    float fontSize;
    int ascent, descent, lineGap;
    float scale;
    int bitmapWidth, bitmapHeight;
    std::vector<uint8_t> bitmap;

    Impl(const std::string& path, float fontSize) : fontSize(fontSize) {
        std::vector<std::string> searchPaths = {
            path,
            "include/fonts/calibri.ttf",
            "include/fonts/arial.ttf",
            "../../include/fonts/calibri.ttf",
            "../../include/fonts/arial.ttf",
            "../include/fonts/calibri.ttf",
            "../include/fonts/arial.ttf",
            "fonts/calibri.ttf",
            "fonts/arial.ttf",
            "C:/Windows/Fonts/calibri.ttf",
            "C:/Windows/Fonts/arial.ttf"
        };

        std::ifstream file;
        std::string usedPath;
        for (const auto& p : searchPaths) {
            file.open(p, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                usedPath = p;
                break;
            }
        }
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open any font file. Searched paths include: " + path);
        }

        size_t size = file.tellg();
        file.seekg(0);
        fontBuffer.resize(size);
        file.read(reinterpret_cast<char*>(fontBuffer.data()), size);
        std::cout << "[Font] Loaded font from: " << usedPath << " (" << size << " bytes)" << std::endl;

        // Bake font bitmap
        bitmapWidth = 512;
        bitmapHeight = 512;
        bitmap.resize(bitmapWidth * bitmapHeight, 0);
        int result = stbtt_BakeFontBitmap(fontBuffer.data(), 0, fontSize,
                                          bitmap.data(), bitmapWidth, bitmapHeight,
                                          32, 96, cdata);
        if (result <= 0) {
            throw std::runtime_error("Failed to bake font bitmap");
        }
        std::cout << "[Font] Baked " << result << " characters" << std::endl;

        stbtt_fontinfo info;
        if (!stbtt_InitFont(&info, fontBuffer.data(), 0)) {
            throw std::runtime_error("Failed to initialize font for metrics");
        }
        scale = stbtt_ScaleForPixelHeight(&info, fontSize);
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    }

    std::shared_ptr<TextureAsset> renderText(const std::string& text, uint32_t color) {
        if (text.empty()) {
            std::vector<uint8_t> dummy(4 * 4 * 4, 0);
            return std::make_shared<TextureAsset>(4, 4, dummy);
        }

        // Измеряем текст
        float x = 0, y = 0;
        stbtt_aligned_quad q;
        float maxX = 0, maxY = 0;
        for (char c : text) {
            if (c < 32 || c > 126) continue;
            stbtt_GetBakedQuad(cdata, bitmapWidth, bitmapHeight, c - 32, &x, &y, &q, 1);
            maxX = std::max(maxX, q.x1);
            maxY = std::max(maxY, q.y1);
        }
        int width = static_cast<int>(maxX + 2);
        int height = static_cast<int>(maxY + 2);
        if (width <= 0) width = 4;
        if (height <= 0) height = 4;

        std::vector<uint8_t> pixels(width * height * 4, 0);
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;

        // Рендерим символы
        x = 0;
        y = fontSize * 1.2f;
        for (char c : text) {
            if (c < 32 || c > 126) continue;
            stbtt_GetBakedQuad(cdata, bitmapWidth, bitmapHeight, c - 32, &x, &y, &q, 1);
            int x0 = static_cast<int>(q.x0);
            int y0 = static_cast<int>(q.y0);
            int x1 = static_cast<int>(q.x1);
            int y1 = static_cast<int>(q.y1);
            if (x1 <= x0 || y1 <= y0) continue;
            for (int py = y0; py < y1; ++py) {
                for (int px = x0; px < x1; ++px) {
                    if (px < 0 || px >= width || py < 0 || py >= height) continue;
                    // Координаты в атласе
                    float s = (px - q.x0) / (q.x1 - q.x0);
                    float t = (py - q.y0) / (q.y1 - q.y0);
                    int atlasX = static_cast<int>(q.s0 + s * (q.s1 - q.s0));
                    int atlasY = static_cast<int>(q.t0 + t * (q.t1 - q.t0));
                    if (atlasX < 0 || atlasX >= bitmapWidth || atlasY < 0 || atlasY >= bitmapHeight) continue;
                    uint8_t alpha = bitmap[atlasY * bitmapWidth + atlasX];
                    if (alpha > 0) {
                        size_t idx = (py * width + px) * 4;
                        pixels[idx + 0] = r;
                        pixels[idx + 1] = g;
                        pixels[idx + 2] = b;
                        pixels[idx + 3] = alpha;
                    }
                }
            }
        }
        std::cout << "[Font] Rendered text \"" << text << "\" size: " << width << "x" << height << std::endl;
        return std::make_shared<TextureAsset>(width, height, pixels);
    }
};

Font::Font(const std::string& path, float fontSize)
    : pImpl(std::make_unique<Impl>(path, fontSize)) {}
Font::~Font() = default;

std::shared_ptr<TextureAsset> Font::renderText(const std::string& text, uint32_t color) {
    return pImpl->renderText(text, color);
}

} // namespace arxglue::ui