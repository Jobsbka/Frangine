#pragma once
#include "arxrender_types.hpp"
#include "arxrender_device.hpp"
#include <memory>
#include <vector>

namespace arxrender {

struct AR_buffer_desc {
    size_t size = 0;
    AR_usage usage = AR_usage::VERTEX_BUFFER;
    const void* initial_data = nullptr;
    bool host_visible = false; // для CPU-апдейтов (UI, динамическая геометрия)
};

class AR_buffer {
public:
    AR_API AR_buffer();
    AR_API ~AR_buffer();
    AR_API void* map();
    AR_API void unmap();
    AR_API void update(const void* data, size_t size, size_t offset = 0);
    AR_API size_t size() const;

private:
    struct impl;
    std::unique_ptr<impl> p_impl;
};

struct AR_texture_desc {
    uint32_t width = 1, height = 1, depth = 1;
    AR_format format = AR_format::RGBA8_UNORM;
    AR_usage usage = AR_usage::SAMPLED_TEXTURE;
    const void* initial_data = nullptr;
    size_t mip_levels = 1;
    bool generate_mips_on_create = false;
};

class AR_texture {
public:
    AR_API AR_texture();
    AR_API ~AR_texture();
    AR_API uint32_t width() const;
    AR_API uint32_t height() const;
    AR_API AR_format format() const;
    AR_API void update_region(const void* data, uint32_t x, uint32_t y, uint32_t w, uint32_t h);

private:
    struct impl;
    std::unique_ptr<impl> p_impl;
};

} // namespace arxrender