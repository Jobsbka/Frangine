#pragma once
#include "arxrender_types.hpp"
#include "arxrender_context.hpp"
#include <memory>
#include <cstdint>

namespace arxrender {
class AR_command_buffer;

struct AR_surface_config {
    void* native_window_handle = nullptr;
    uint32_t width = 800, height = 600;
    AR_format format = AR_format::RGBA8_UNORM;
    bool vsync = true;
    uint32_t swapchain_image_count = 2;
};

class AR_surface {
public:
    AR_API AR_surface();
    AR_API ~AR_surface();
    AR_API AR_result init(AR_context* ctx, const AR_surface_config* config);
    AR_API void uninit();
    AR_API AR_result resize(uint32_t w, uint32_t h);
    AR_API AR_result present(AR_command_buffer* cmd);
    AR_API uint32_t width() const;
    AR_API uint32_t height() const;
    AR_API void* backend_data() const;
    AR_API void set_backend_data(void* data);

private:
    struct impl;
    std::unique_ptr<impl> p_impl;
};