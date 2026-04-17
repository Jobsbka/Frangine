#pragma once
#include "arxrender_context.hpp"
#include "arxrender_device.hpp"
#include "arxrender_surface.hpp"
#include "arxrender_command.hpp"
#include "arxrender_resource.hpp"
#include "arxrender_pipeline.hpp"

namespace arxrender {

struct AR_backend_callbacks {
    AR_result (*onContextInit)(AR_context*, const AR_context_config*);
    AR_result (*onContextUninit)(AR_context*);
    AR_result (*onDeviceInit)(AR_context*, const AR_device_config*, AR_device*);
    AR_result (*onDeviceUninit)(AR_device*);
    AR_result (*onDeviceStart)(AR_device*);
    AR_result (*onDeviceStop)(AR_device*);
    AR_result (*onSurfaceInit)(AR_context*, const AR_surface_config*, AR_surface*);
    AR_result (*onSurfaceUninit)(AR_surface*);
    AR_result (*onSurfaceResize)(AR_surface*, uint32_t, uint32_t);
    AR_result (*onSurfacePresent)(AR_surface*, AR_command_buffer*);
    AR_result (*onBufferCreate)(AR_device*, const AR_buffer_desc*, AR_buffer**);
    AR_result (*onBufferDestroy)(AR_buffer*);
    AR_result (*onTextureCreate)(AR_device*, const AR_texture_desc*, AR_texture**);
    AR_result (*onTextureDestroy)(AR_texture*);
    AR_result (*onShaderCreate)(AR_device*, AR_shader_stage, const void*, size_t, const char*, AR_shader**);
    AR_result (*onShaderDestroy)(AR_shader*);
    AR_result (*onPipelineCreate)(AR_device*, const AR_pipeline_desc*, AR_pipeline**);
    AR_result (*onPipelineDestroy)(AR_pipeline*);
    AR_result (*onCmdBufferCreate)(AR_device*, AR_command_buffer**);
    AR_result (*onCmdBufferDestroy)(AR_command_buffer*);
    AR_result (*onCmdBegin)(AR_command_buffer*);
    AR_result (*onCmdEnd)(AR_command_buffer*);
    AR_result (*onCmdExecute)(AR_command_buffer*);
    void (*onCmdClear)(AR_command_buffer*, float, float, float, float);
    void (*onCmdSetViewport)(AR_command_buffer*, float, float, float, float);
    void (*onCmdBindPipeline)(AR_command_buffer*, AR_pipeline*);
    void (*onCmdBindMaterial)(AR_command_buffer*, AR_material*);
    void (*onCmdDraw)(AR_command_buffer*, uint32_t, uint32_t, uint32_t, uint32_t);
    void (*onCmdDrawIndexed)(AR_command_buffer*, AR_buffer*, uint32_t, uint32_t, uint32_t, int32_t, uint32_t);
};

struct AR_backend_info {
    AR_backend backend;
    const char* name;
    bool (*is_available)();
    AR_backend_callbacks* callbacks;
};

AR_API AR_result AR_register_backend(const AR_backend_info* info);
}