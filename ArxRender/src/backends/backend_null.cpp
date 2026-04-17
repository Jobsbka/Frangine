#include "../include/arxrender_backend.hpp"
#include <vector>
#include <cstring>
#include <iostream>

namespace arxrender {

struct NullDeviceState {};
struct NullSurfaceState { uint32_t w=0, h=0; };
struct NullBufferState { size_t size=0; AR_usage usage=AR_usage(0); std::vector<uint8_t> data; };
struct NullTextureState { uint32_t w=1, h=1; AR_format fmt=AR_format::UNDEFINED; };
struct NullShaderState { AR_shader_stage stage=AR_shader_stage::VERTEX; std::vector<uint8_t> code; };
struct NullPipelineState { AR_primitive_topology topo=AR_primitive_topology::TRIANGLE_LIST; };
struct NullMaterialState { std::vector<uint8_t> dummy; };

enum class NullCmdType { Clear, Viewport, BindPipeline, BindMaterial, Draw, DrawIndexed };
struct NullCommand {
    NullCmdType type = NullCmdType::Clear;
    union Data {
        float clear[4];
        float vp[4];
        NullPipelineState* pipe;
        NullMaterialState* mat;
        struct { uint32_t vc, ic, fv, fi; } draw;
        struct { NullBufferState* idx; uint32_t ic, inst, fi, vo; uint32_t fii; } drawIdx;
        Data() { memset(this, 0, sizeof(*this)); }
    } data;
};
struct NullCmdState { bool recording=false; std::vector<NullCommand> cmds; };

template<typename T> static T* make_null() { return new(std::nothrow) T{}; }

static AR_result null_onContextInit(AR_context*, const AR_context_config*) { return AR_result::SUCCESS; }
static AR_result null_onContextUninit(AR_context*) { return AR_result::SUCCESS; }
static AR_result null_onDeviceInit(AR_context*, const AR_device_config*, AR_device** out) {
    *out = new(std::nothrow) AR_device{};
    if(*out) (*out)->set_backend_data(new NullDeviceState{});
    return *out ? AR_result::SUCCESS : AR_result::OUT_OF_MEMORY;
}
static AR_result null_onDeviceUninit(AR_device* dev) {
    if(dev) delete static_cast<NullDeviceState*>(dev->backend_data());
    delete dev;
    return AR_result::SUCCESS;
}
static AR_result null_onDeviceStart(AR_device*) { return AR_result::SUCCESS; }
static AR_result null_onDeviceStop(AR_device*) { return AR_result::SUCCESS; }
static AR_result null_onSurfaceInit(AR_context*, const AR_surface_config* cfg, AR_surface** out) {
    *out = new(std::nothrow) AR_surface{};
    if(*out) (*out)->set_backend_data(new NullSurfaceState{cfg->width, cfg->height});
    return *out ? AR_result::SUCCESS : AR_result::OUT_OF_MEMORY;
}
static AR_result null_onSurfaceUninit(AR_surface* surf) {
    if(surf) delete static_cast<NullSurfaceState*>(surf->backend_data());
    delete surf;
    return AR_result::SUCCESS;
}
static AR_result null_onSurfaceResize(AR_surface* surf, uint32_t w, uint32_t h) {
    auto* s = static_cast<NullSurfaceState*>(surf->backend_data());
    if(s) { s->w = w; s->h = h; }
    return AR_result::SUCCESS;
}
static AR_result null_onSurfacePresent(AR_surface*, AR_command_buffer*) { return AR_result::SUCCESS; }
static AR_result null_onBufferCreate(AR_device*, const AR_buffer_desc* desc, AR_buffer** out) {
    auto* buf = make_null<NullBufferState>();
    if(!buf) return AR_result::OUT_OF_MEMORY;
    buf->size = desc->size; buf->usage = desc->usage;
    if(desc->initial_data) buf->data.assign(static_cast<const uint8_t*>(desc->initial_data), static_cast<const uint8_t*>(desc->initial_data)+desc->size);
    *out = reinterpret_cast<AR_buffer*>(buf);
    return AR_result::SUCCESS;
}
static AR_result null_onBufferDestroy(AR_buffer* buf) { delete reinterpret_cast<NullBufferState*>(buf); return AR_result::SUCCESS; }
static AR_result null_onTextureCreate(AR_device*, const AR_texture_desc* desc, AR_texture** out) {
    auto* tex = make_null<NullTextureState>();
    if(!tex) return AR_result::OUT_OF_MEMORY;
    tex->w = desc->width; tex->h = desc->height; tex->fmt = desc->format;
    *out = reinterpret_cast<AR_texture*>(tex);
    return AR_result::SUCCESS;
}
static AR_result null_onTextureDestroy(AR_texture* tex) { delete reinterpret_cast<NullTextureState*>(tex); return AR_result::SUCCESS; }
static AR_result null_onShaderCreate(AR_device*, AR_shader_stage stage, const void* code, size_t size, const char*, AR_shader** out) {
    auto* sh = make_null<NullShaderState>();
    if(!sh) return AR_result::OUT_OF_MEMORY;
    sh->stage = stage;
    if(code && size>0) sh->code.assign(static_cast<const uint8_t*>(code), static_cast<const uint8_t*>(code)+size);
    *out = reinterpret_cast<AR_shader*>(sh);
    return AR_result::SUCCESS;
}
static AR_result null_onShaderDestroy(AR_shader* sh) { delete reinterpret_cast<NullShaderState*>(sh); return AR_result::SUCCESS; }
static AR_result null_onPipelineCreate(AR_device*, const AR_pipeline_desc* desc, AR_pipeline** out) {
    auto* p = make_null<NullPipelineState>();
    if(!p) return AR_result::OUT_OF_MEMORY;
    p->topo = desc->topology;
    *out = reinterpret_cast<AR_pipeline*>(p);
    return AR_result::SUCCESS;
}
static AR_result null_onPipelineDestroy(AR_pipeline* pipe) { delete reinterpret_cast<NullPipelineState*>(pipe); return AR_result::SUCCESS; }
static AR_result null_onCmdBufferCreate(AR_device*, AR_command_buffer** out) {
    *out = reinterpret_cast<AR_command_buffer*>(make_null<NullCmdState>());
    return *out ? AR_result::SUCCESS : AR_result::OUT_OF_MEMORY;
}
static AR_result null_onCmdBufferDestroy(AR_command_buffer* cmd) { delete reinterpret_cast<NullCmdState*>(cmd); return AR_result::SUCCESS; }
static AR_result null_onCmdBegin(AR_command_buffer* cmd) {
    auto* c = reinterpret_cast<NullCmdState*>(cmd);
    if(c->recording) return AR_result::INVALID_OPERATION;
    c->recording = true; c->cmds.clear();
    return AR_result::SUCCESS;
}
static AR_result null_onCmdEnd(AR_command_buffer* cmd) {
    auto* c = reinterpret_cast<NullCmdState*>(cmd);
    if(!c->recording) return AR_result::INVALID_OPERATION;
    c->recording = false;
    return AR_result::SUCCESS;
}
static AR_result null_onCmdExecute(AR_command_buffer* cmd) {
    auto* c = reinterpret_cast<NullCmdState*>(cmd);
    std::cout << "[NullRender] Executing " << c->cmds.size() << " commands\n";
    return AR_result::SUCCESS;
}
static void null_onCmdClear(AR_command_buffer* cmd, float r, float g, float b, float a) {
    auto* c = reinterpret_cast<NullCmdState*>(cmd);
    if(c->recording) { NullCommand nc; nc.type = NullCmdType::Clear; nc.data.clear[0]=r; nc.data.clear[1]=g; nc.data.clear[2]=b; nc.data.clear[3]=a; c->cmds.push_back(nc); }
}
static void null_onCmdSetViewport(AR_command_buffer* cmd, float x, float y, float w, float h) {
    auto* c = reinterpret_cast<NullCmdState*>(cmd);
    if(c->recording) { NullCommand nc; nc.type = NullCmdType::Viewport; nc.data.vp[0]=x; nc.data.vp[1]=y; nc.data.vp[2]=w; nc.data.vp[3]=h; c->cmds.push_back(nc); }
}
static void null_onCmdBindPipeline(AR_command_buffer* cmd, AR_pipeline* pipe) {
    auto* c = reinterpret_cast<NullCmdState*>(cmd);
    if(c->recording) { NullCommand nc; nc.type = NullCmdType::BindPipeline; nc.data.pipe = reinterpret_cast<NullPipelineState*>(pipe); c->cmds.push_back(nc); }
}
static void null_onCmdBindMaterial(AR_command_buffer* cmd, AR_material* mat) {
    auto* c = reinterpret_cast<NullCmdState*>(cmd);
    if(c->recording) { NullCommand nc; nc.type = NullCmdType::BindMaterial; nc.data.mat = reinterpret_cast<NullMaterialState*>(mat); c->cmds.push_back(nc); }
}
static void null_onCmdDraw(AR_command_buffer* cmd, uint32_t vc, uint32_t ic, uint32_t fv, uint32_t fi) {
    auto* c = reinterpret_cast<NullCmdState*>(cmd);
    if(c->recording) { NullCommand nc; nc.type = NullCmdType::Draw; nc.data.draw = {vc,ic,fv,fi}; c->cmds.push_back(nc); }
}
static void null_onCmdDrawIndexed(AR_command_buffer* cmd, AR_buffer* idx, uint32_t ic, uint32_t inst, uint32_t fi, int32_t vo, uint32_t fii) {
    auto* c = reinterpret_cast<NullCmdState*>(cmd);
    if(c->recording) { NullCommand nc; nc.type = NullCmdType::DrawIndexed; nc.data.drawIdx = {reinterpret_cast<NullBufferState*>(idx), ic, inst, fi, static_cast<uint32_t>(vo), fii}; c->cmds.push_back(nc); }
}

static bool null_is_available() { return true; }
static AR_backend_callbacks g_null_callbacks = {
    null_onContextInit, null_onContextUninit, null_onDeviceInit, null_onDeviceUninit,
    null_onDeviceStart, null_onDeviceStop, null_onSurfaceInit, null_onSurfaceUninit,
    null_onSurfaceResize, null_onSurfacePresent, null_onBufferCreate, null_onBufferDestroy,
    null_onTextureCreate, null_onTextureDestroy, null_onShaderCreate, null_onShaderDestroy,
    null_onPipelineCreate, null_onPipelineDestroy, null_onCmdBufferCreate, null_onCmdBufferDestroy,
    null_onCmdBegin, null_onCmdEnd, null_onCmdExecute, null_onCmdClear, null_onCmdSetViewport,
    null_onCmdBindPipeline, null_onCmdBindMaterial, null_onCmdDraw, null_onCmdDrawIndexed
};
static AR_backend_info g_null_info = { AR_backend::NULL_BACKEND, "Null", null_is_available, &g_null_callbacks };
AR_result AR_register_null_backend() { return AR_register_backend(&g_null_info); }

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void auto_reg_null() { AR_register_null_backend(); }
#elif defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void __cdecl auto_reg_null_msvc() { AR_register_null_backend(); }
__declspec(allocate(".CRT$XCU")) void (*__auto_reg_null)(void) = auto_reg_null_msvc;
#endif
} // namespace arxrender