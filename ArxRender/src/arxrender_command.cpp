#include "../include/arxrender_command.hpp"
#include <mutex>

namespace arxrender {

struct AR_command_buffer::impl {
    AR_device* device = nullptr;
    void* backend_handle = nullptr;
    bool recording = false;
    std::mutex mutex;
};

AR_command_buffer::AR_command_buffer() : p_impl(std::make_unique<impl>()) {}
AR_command_buffer::~AR_command_buffer() {
    if (p_impl && p_impl->device && p_impl->device->context() && p_impl->device->context()->p_impl) {
        auto& cb = p_impl->device->context()->p_impl->callbacks;
        if (cb.onCmdBufferDestroy) cb.onCmdBufferDestroy(this);
    }
}

AR_result AR_command_buffer::begin() {
    if (!p_impl) return AR_result::INVALID_OPERATION;
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    if (p_impl->recording) return AR_result::INVALID_OPERATION;
    auto& cb = p_impl->device->context()->p_impl->callbacks;
    AR_result res = cb.onCmdBegin(this);
    if (res) p_impl->recording = true;
    return res;
}

AR_result AR_command_buffer::end() {
    if (!p_impl) return AR_result::INVALID_OPERATION;
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    if (!p_impl->recording) return AR_result::INVALID_OPERATION;
    p_impl->recording = false;
    return p_impl->device->context()->p_impl->callbacks.onCmdEnd(this);
}

AR_result AR_command_buffer::execute() {
    if (!p_impl || p_impl->recording) return AR_result::INVALID_OPERATION;
    return p_impl->device->context()->p_impl->callbacks.onCmdExecute(this);
}

void AR_command_buffer::clear_color(float r, float g, float b, float a) {
    if (!p_impl || !p_impl->recording) return;
    p_impl->device->context()->p_impl->callbacks.onCmdClear(this, r, g, b, a);
}

void AR_command_buffer::clear_depth(float depth, uint8_t stencil) {
    // Пока не вынесено в колбэки, можно добавить в AR_backend_callbacks при необходимости
    (void)depth; (void)stencil;
}

void AR_command_buffer::set_viewport(float x, float y, float w, float h) {
    if (!p_impl || !p_impl->recording) return;
    p_impl->device->context()->p_impl->callbacks.onCmdSetViewport(this, x, y, w, h);
}

void AR_command_buffer::set_scissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    (void)x; (void)y; (void)w; (void)h; // Аналогично clear_depth
}

void AR_command_buffer::bind_pipeline(AR_pipeline* pipeline) {
    if (!p_impl || !p_impl->recording || !pipeline) return;
    p_impl->device->context()->p_impl->callbacks.onCmdBindPipeline(this, pipeline);
}

void AR_command_buffer::bind_material(AR_material* material) {
    if (!p_impl || !p_impl->recording || !material) return;
    p_impl->device->context()->p_impl->callbacks.onCmdBindMaterial(this, material);
}

void AR_command_buffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
    if (!p_impl || !p_impl->recording) return;
    p_impl->device->context()->p_impl->callbacks.onCmdDraw(this, vertex_count, instance_count, first_vertex, first_instance);
}

void AR_command_buffer::draw_indexed(AR_buffer* index_buf, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
    if (!p_impl || !p_impl->recording || !index_buf) return;
    p_impl->device->context()->p_impl->callbacks.onCmdDrawIndexed(this, index_buf, index_count, instance_count, first_index, vertex_offset, first_instance);
}

} // namespace arxrender