#include "../include/arxrender_pipeline.hpp"
#include <unordered_map>
#include <mutex>

namespace arxrender {

// --- AR_shader ---
struct AR_shader::impl { AR_device* device = nullptr; void* backend_handle = nullptr; };
AR_shader::AR_shader() : p_impl(std::make_unique<impl>()) {}
AR_shader::~AR_shader() {
    if (p_impl && p_impl->device && p_impl->device->context() && p_impl->device->context()->p_impl) {
        auto& cb = p_impl->device->context()->p_impl->callbacks;
        if (cb.onShaderDestroy) cb.onShaderDestroy(this);
    }
}

// --- AR_pipeline ---
struct AR_pipeline::impl { AR_device* device = nullptr; void* backend_handle = nullptr; };
AR_pipeline::AR_pipeline() : p_impl(std::make_unique<impl>()) {}
AR_pipeline::~AR_pipeline() {
    if (p_impl && p_impl->device && p_impl->device->context() && p_impl->device->context()->p_impl) {
        auto& cb = p_impl->device->context()->p_impl->callbacks;
        if (cb.onPipelineDestroy) cb.onPipelineDestroy(this);
    }
}

// --- AR_material ---
struct AR_material::impl {
    AR_pipeline* pipeline = nullptr;
    std::unordered_map<std::string, AR_texture*> textures;
    std::unordered_map<std::string, std::any> uniforms;
    std::mutex mutex;
};

AR_material::AR_material(AR_pipeline* pipeline) : p_impl(std::make_unique<impl>()) {
    if (p_impl) p_impl->pipeline = pipeline;
}
AR_material::~AR_material() = default;

void AR_material::set_texture(const std::string& name, AR_texture* tex) {
    if (!p_impl) return;
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    p_impl->textures[name] = tex;
}

void AR_material::set_uniform(const std::string& name, const std::any& value) {
    if (!p_impl) return;
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    p_impl->uniforms[name] = value;
}

AR_pipeline* AR_material::pipeline() const {
    return p_impl ? p_impl->pipeline : nullptr;
}

} // namespace arxrender