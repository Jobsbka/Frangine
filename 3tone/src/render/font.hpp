// src/render/font.hpp
#pragma once
#include <string>
#include <memory>
#include <cstdint>

namespace arxglue {
    class TextureAsset;
}

namespace arxglue::ui {

class Font {
public:
    Font(const std::string& path, float fontSize);
    ~Font();

    std::shared_ptr<TextureAsset> renderText(const std::string& text, uint32_t color);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace arxglue::ui