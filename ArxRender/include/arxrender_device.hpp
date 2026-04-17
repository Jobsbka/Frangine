#pragma once
#include "arxrender_types.hpp"
#include "arxrender_context.hpp"
#include <memory>

namespace arxrender {
class AR_buffer;
class AR_texture;
class AR_shader;
class AR_pipeline;
class AR_command_buffer;

struct AR_device_config {
    uint32_t adapter_index = 0;
    bool debug_layer = false;
};

class AR_device {
public:
    AR_API AR_device();
    AR_API ~AR_device();
    AR_API AR_result init(AR_context* ctx, const AR_device_config* config = nullptr);
    AR_API void uninit();
    AR_API AR_result create_buffer(const struct AR_buffer_desc& desc, AR_buffer** out);
    AR_API AR_result create_texture(const struct AR_texture_desc& desc, AR_texture** out);
    AR_API AR_result create_shader(AR_shader_stage stage, const void* code, size_t size, const char* entry, AR_shader** out);
    AR_API AR_result create_pipeline(const struct AR_pipeline_desc& desc, AR_pipeline** out);
    AR_API AR_result create_command_buffer(AR_command_buffer** out);
    AR_API void wait_idle() const;
    AR_API AR_log* log() const;
    AR_API AR_context* context() const;
    AR_API void* backend_data() const;
    AR_API void set_backend_data(void* data);

private:
    struct impl;
    std::unique_ptr<impl> p_impl;
};