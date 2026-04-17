#include "../include/arxrender_device.hpp"
#include "../include/arxrender_command.hpp"
#include <mutex>

namespace arxrender {
struct AR_device::impl {
    AR_context* context = nullptr;
    AR_device_config config{};
    void* backend_data = nullptr;
    bool initialized = false;
    std::mutex mutex;
};

AR_device::AR_device() : p_impl(std::make_unique<impl>()) {}
AR_device::~AR_device() = default;

AR_result AR_device::init(AR_context* ctx, const AR_device_config* config) {
    if (!p_impl || !ctx || !config) return AR_result::INVALID_ARGS;
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    if (p_impl->initialized) return AR_result::INVALID_OPERATION;
    p_impl->context = ctx;
    p_impl->config = *config;

    auto* cb = ctx->callbacks();
    if (!cb || !cb->onDeviceInit) return AR_result::NOT_IMPLEMENTED;
    AR_result res = cb->onDeviceInit(ctx, config, this);
    if (res != AR_result::SUCCESS) return res;
    p_impl->initialized = true;
    return AR_result::SUCCESS;
}

void AR_device::uninit() {
    if (!p_impl) return;
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    if (!p_impl->initialized) return;
    auto* cb = p_impl->context ? p_impl->context->callbacks() : nullptr;
    if (cb && cb->onDeviceUninit) cb->onDeviceUninit(this);
    p_impl->initialized = false;
    p_impl->backend_data = nullptr;
}

AR_result AR_device::create_buffer(const AR_buffer_desc& desc, AR_buffer** out) {
    if (!p_impl || !out) return AR_result::INVALID_ARGS;
    auto* cb = p_impl->context ? p_impl->context->callbacks() : nullptr;
    if (!cb || !cb->onBufferCreate) return AR_result::NOT_IMPLEMENTED;
    return cb->onBufferCreate(this, &desc, out);
}
AR_result AR_device::create_texture(const AR_texture_desc& desc, AR_texture** out) {
    if (!p_impl || !out) return AR_result::INVALID_ARGS;
    auto* cb = p_impl->context ? p_impl->context->callbacks() : nullptr;
    if (!cb || !cb->onTextureCreate) return AR_result::NOT_IMPLEMENTED;
    return cb->onTextureCreate(this, &desc, out);
}
AR_result AR_device::create_shader(AR_shader_stage stage, const void* code, size_t size, const char* entry, AR_shader** out) {
    if (!p_impl || !out) return AR_result::INVALID_ARGS;
    auto* cb = p_impl->context ? p_impl->context->callbacks() : nullptr;
    if (!cb || !cb->onShaderCreate) return AR_result::NOT_IMPLEMENTED;
    return cb->onShaderCreate(this, stage, code, size, entry, out);
}
AR_result AR_device::create_pipeline(const AR_pipeline_desc& desc, AR_pipeline** out) {
    if (!p_impl || !out) return AR_result::INVALID_ARGS;
    auto* cb = p_impl->context ? p_impl->context->callbacks() : nullptr;
    if (!cb || !cb->onPipelineCreate) return AR_result::NOT_IMPLEMENTED;
    return cb->onPipelineCreate(this, &desc, out);
}
AR_result AR_device::create_command_buffer(AR_command_buffer** out) {
    if (!p_impl || !out) return AR_result::INVALID_ARGS;
    auto* cb = p_impl->context ? p_impl->context->callbacks() : nullptr;
    if (!cb || !cb->onCmdBufferCreate) return AR_result::NOT_IMPLEMENTED;
    return cb->onCmdBufferCreate(this, out);
}

void AR_device::wait_idle() const {}
AR_log* AR_device::log() const { return p_impl ? p_impl->context->log() : nullptr; }
AR_context* AR_device::context() const { return p_impl ? p_impl->context : nullptr; }
void* AR_device::backend_data() const { return p_impl ? p_impl->backend_data : nullptr; }
void AR_device::set_backend_data(void* data) { if(p_impl) p_impl->backend_data = data; }
} // namespace arxrender