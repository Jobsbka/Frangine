#include "../include/arxrender_resource.hpp"
#include <mutex>

namespace arxrender {

// --- AR_buffer ---
struct AR_buffer::impl {
    AR_device* device = nullptr;
    void* backend_handle = nullptr;
    bool mapped = false;
    size_t size = 0;
    std::mutex mutex;
};

AR_buffer::AR_buffer() : p_impl(std::make_unique<impl>()) {}
AR_buffer::~AR_buffer() {
    if (p_impl && p_impl->device && p_impl->device->p_impl) {
        auto* ctx = p_impl->device->context();
        if (ctx && ctx->p_impl && ctx->p_impl->callbacks.onBufferDestroy) {
            ctx->p_impl->callbacks.onBufferDestroy(this);
        }
    }
}
void* AR_buffer::map() {
    if (!p_impl) return nullptr;
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    if (p_impl->mapped) return nullptr;
    p_impl->mapped = true;
    // В реальных бэкендах: vkMapMemory / glMapBuffer
    return nullptr; 
}
void AR_buffer::unmap() {
    if (p_impl) p_impl->mapped = false;
}
void AR_buffer::update(const void* data, size_t size, size_t offset) {
    if (!p_impl || !data) return;
    // Бэкенд сам выберет стратегию: staged upload, map-unmap или direct sub-data
}
size_t AR_buffer::size() const { return p_impl ? p_impl->size : 0; }

// --- AR_texture ---
struct AR_texture::impl {
    AR_device* device = nullptr;
    void* backend_handle = nullptr;
    uint32_t w = 0, h = 0, d = 1;
    AR_format fmt = AR_format::UNDEFINED;
    std::mutex mutex;
};

AR_texture::AR_texture() : p_impl(std::make_unique<impl>()) {}
AR_texture::~AR_texture() {
    if (p_impl && p_impl->device && p_impl->device->context() && p_impl->device->context()->p_impl) {
        auto& cb = p_impl->device->context()->p_impl->callbacks;
        if (cb.onTextureDestroy) cb.onTextureDestroy(this);
    }
}
uint32_t AR_texture::width() const { return p_impl ? p_impl->w : 0; }
uint32_t AR_texture::height() const { return p_impl ? p_impl->h : 0; }
AR_format AR_texture::format() const { return p_impl ? p_impl->fmt : AR_format::UNDEFINED; }
void AR_texture::update_region(const void* data, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!p_impl || !data) return;
    // Бэкенд: glTexSubImage2D / vkCmdUpdateBuffer / vkQueueSubmit staging
}

} // namespace arxrender