#include "../include/arxrender_surface.hpp"
#include <mutex>

namespace arxrender {

struct AR_surface::impl {
    AR_context* context = nullptr;
    AR_surface_config config{};
    void* backend_data = nullptr;
    bool initialized = false;
    std::mutex mutex;
};

AR_surface::AR_surface() : p_impl(std::make_unique<impl>()) {}
AR_surface::~AR_surface() = default;

AR_result AR_surface::init(AR_context* ctx, const AR_surface_config* config) {
    if (!p_impl || !ctx || !config) return AR_result::INVALID_ARGS;
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    if (p_impl->initialized) return AR_result::INVALID_OPERATION;
    p_impl->context = ctx;
    p_impl->config = *config;
    auto* ctx_impl = ctx->p_impl.get();
    if (!ctx_impl || !ctx_impl->callbacks.onSurfaceCreate) return AR_result::NOT_IMPLEMENTED;
    AR_result res = ctx_impl->callbacks.onSurfaceCreate(ctx, config, this);
    if (res) p_impl->initialized = true;
    return res;
}

void AR_surface::uninit() {
    if (!p_impl) return;
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    if (!p_impl->initialized) return;
    auto* ctx_impl = p_impl->context->p_impl.get();
    if (ctx_impl && ctx_impl->callbacks.onSurfaceDestroy) ctx_impl->callbacks.onSurfaceDestroy(this);
    p_impl->initialized = false;
}

AR_result AR_surface::resize(uint32_t w, uint32_t h) {
    if (!p_impl || !p_impl->initialized) return AR_result::INVALID_OPERATION;
    p_impl->config.width = w;
    p_impl->config.height = h;
    auto* ctx_impl = p_impl->context->p_impl.get();
    return ctx_impl ? ctx_impl->callbacks.onSurfaceResize(this, w, h) : AR_result::NOT_IMPLEMENTED;
}

AR_result AR_surface::present(AR_command_buffer* cmd) {
    if (!p_impl || !p_impl->initialized) return AR_result::INVALID_OPERATION;
    auto* ctx_impl = p_impl->context->p_impl.get();
    return ctx_impl ? ctx_impl->callbacks.onSurfacePresent(this, cmd) : AR_result::NOT_IMPLEMENTED;
}

uint32_t AR_surface::width() const { return p_impl ? p_impl->config.width : 0; }
uint32_t AR_surface::height() const { return p_impl ? p_impl->config.height : 0; }

} // namespace arxrender