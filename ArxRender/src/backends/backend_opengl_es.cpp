#include "../include/arxrender_backend.hpp"
#if defined(ARXRENDER_USE_WEBGL) || defined(__EMSCRIPTEN__)
    #include <GLES3/gl3.h>
    #include <emscripten/html5.h>
    #include <emscripten/webgl.h>
    #define ARX_GL_WEB 1
#else
    #include <glad/glad.h>
    #define GLFW_INCLUDE_NONE
    #include <GLFW/glfw3.h>
    #define ARX_GL_DESKTOP 1
#endif
#include <vector>
#include <mutex>
#include <unordered_map>
#include <any>
#include <array>
#include <iostream>

namespace arxrender {
#if ARX_GL_DESKTOP
static void ensure_glad(GLFWwindow* win) {
    if (!win) return;
    static std::once_flag f;
    std::call_once(f, [&](){
        glfwMakeContextCurrent(win);
        if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) throw std::runtime_error("GLAD failed");
        glEnable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    });
}
#endif

struct GL_DeviceState { void* win = nullptr; };
struct GL_SurfaceState { void* win = nullptr; uint32_t w=0, h=0; };
struct GL_BufferState { GLuint id=0; GLenum target=GL_ARRAY_BUFFER; size_t size=0; AR_usage usage=AR_usage(0); };
struct GL_TextureState { GLuint id=0; uint32_t w=1, h=1; };
struct GL_ShaderState { GLuint id=0; };
struct GL_PipelineState { GLuint prog=0, vao=0; AR_primitive_topology topo=AR_primitive_topology::TRIANGLE_LIST; AR_cull_mode cull=AR_cull_mode::BACK; bool depth_test=false, depth_write=true; AR_compare_op depth_cmp=AR_compare_op::LESS; };
struct GL_MaterialState { std::unordered_map<std::string, std::any> uniforms; };

enum class GLCmd { Clear, Viewport, BindPipe, BindMat, Draw, DrawIdx };
struct GLCommand {
    GLCmd type = GLCmd::Clear;
    union Data {
        float clear[4]; float vp[4];
        GL_PipelineState* pipe; GL_MaterialState* mat;
        struct { uint32_t vc, ic, fv, fi; } draw;
        struct { GLuint idx; uint32_t ic, inst, fi, vo; uint32_t fii; } drawIdx;
        Data() { memset(this, 0, sizeof(*this)); }
    } data;
};
struct GL_CmdState { bool recording=false; std::vector<GLCommand> cmds; };

static GLenum gl_topo(AR_primitive_topology t) { switch(t){case AR_primitive_topology::POINT_LIST:return GL_POINTS;case AR_primitive_topology::LINE_LIST:return GL_LINES;default:return GL_TRIANGLES;} }
static GLenum gl_cmp(AR_compare_op o) { switch(o){case AR_compare_op::LEQUAL:return GL_LEQUAL;case AR_compare_op::ALWAYS:return GL_ALWAYS;default:return GL_LESS;} }

static AR_result gl_onContextInit(AR_context*, const AR_context_config*) { return AR_result::SUCCESS; }
static AR_result gl_onContextUninit(AR_context*) { return AR_result::SUCCESS; }
static AR_result gl_onDeviceInit(AR_context*, const AR_device_config*, AR_device* device) {
    device->set_backend_data(new GL_DeviceState{});
    return AR_result::SUCCESS;
}
static AR_result gl_onDeviceUninit(AR_device* d) { delete static_cast<GL_DeviceState*>(d->backend_data()); delete d; return AR_result::SUCCESS; }
static AR_result gl_onDeviceStart(AR_device*) { return AR_result::SUCCESS; }
static AR_result gl_onDeviceStop(AR_device*) { return AR_result::SUCCESS; }
static AR_result gl_onSurfaceInit(AR_context*, const AR_surface_config* cfg, AR_surface* surface) {
    auto* st = new GL_SurfaceState{cfg->native_window_handle, cfg->width, cfg->height};
    surface->set_backend_data(st);
#if ARX_GL_DESKTOP
    if(st->win) { glfwSetWindowSize(static_cast<GLFWwindow*>(st->win), st->w, st->h); ensure_glad(static_cast<GLFWwindow*>(st->win)); }
#endif
    glViewport(0, 0, st->w, st->h);
    return AR_result::SUCCESS;
}
static AR_result gl_onSurfaceUninit(AR_surface* s) { delete static_cast<GL_SurfaceState*>(s->backend_data()); delete s; return AR_result::SUCCESS; }
static AR_result gl_onSurfaceResize(AR_surface* s, uint32_t w, uint32_t h) {
    auto* st = static_cast<GL_SurfaceState*>(s->backend_data());
    st->w=w; st->h=h; glViewport(0,0,w,h);
#if ARX_GL_DESKTOP
    if(st->win) glfwSetWindowSize(static_cast<GLFWwindow*>(st->win), w, h);
#endif
    return AR_result::SUCCESS;
}
static AR_result gl_onSurfacePresent(AR_surface* s, AR_command_buffer*) {
#if ARX_GL_DESKTOP
    auto* st = static_cast<GL_SurfaceState*>(s->backend_data());
    if(st && st->win) glfwSwapBuffers(static_cast<GLFWwindow*>(st->win));
#endif
    return AR_result::SUCCESS;
}
static AR_result gl_onBufferCreate(AR_device*, const AR_buffer_desc* desc, AR_buffer** out) {
    auto* b = new GL_BufferState{}; b->size=desc->size; b->usage=desc->usage;
    b->target = (desc->usage & AR_usage::INDEX_BUFFER) ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    glGenBuffers(1, &b->id); glBindBuffer(b->target, b->id); glBufferData(b->target, desc->size, desc->initial_data, GL_STATIC_DRAW); glBindBuffer(b->target, 0);
    *out = reinterpret_cast<AR_buffer*>(b);
    return AR_result::SUCCESS;
}
static AR_result gl_onBufferDestroy(AR_buffer* buf) { auto* b=reinterpret_cast<GL_BufferState*>(buf); if(b->id) glDeleteBuffers(1,&b->id); delete b; return AR_result::SUCCESS; }
static AR_result gl_onTextureCreate(AR_device*, const AR_texture_desc* desc, AR_texture** out) {
    auto* t = new GL_TextureState{0, desc->width, desc->height};
    glGenTextures(1, &t->id); glBindTexture(GL_TEXTURE_2D, t->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, desc->width, desc->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, desc->initial_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, desc->mip_levels>1?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if(desc->initial_data && desc->mip_levels>1) glGenerateMipmap(GL_TEXTURE_2D);
    *out = reinterpret_cast<AR_texture*>(t);
    return AR_result::SUCCESS;
}
static AR_result gl_onTextureDestroy(AR_texture* tex) { auto* t=reinterpret_cast<GL_TextureState*>(tex); if(t->id) glDeleteTextures(1,&t->id); delete t; return AR_result::SUCCESS; }
static AR_result gl_onShaderCreate(AR_device*, AR_shader_stage stage, const void* code, size_t, const char*, AR_shader** out) {
    auto* sh = new GL_ShaderState{};
    GLenum type = (stage==AR_shader_stage::VERTEX)?GL_VERTEX_SHADER:GL_FRAGMENT_SHADER;
    sh->id = glCreateShader(type);
    const char* src = static_cast<const char*>(code);
    glShaderSource(sh->id, 1, &src, nullptr); glCompileShader(sh->id);
    GLint ok; glGetShaderiv(sh->id, GL_COMPILE_STATUS, &ok);
    if(!ok) { char log[512]; glGetShaderInfoLog(sh->id, sizeof(log), nullptr, log); std::cerr<<"[GL] Shader error: "<<log<<"\n"; glDeleteShader(sh->id); delete sh; return AR_result::PIPELINE_COMPILE_FAILED; }
    *out = reinterpret_cast<AR_shader*>(sh);
    return AR_result::SUCCESS;
}
static AR_result gl_onShaderDestroy(AR_shader* sh) { auto* s=reinterpret_cast<GL_ShaderState*>(sh); if(s->id) glDeleteShader(s->id); delete s; return AR_result::SUCCESS; }
static AR_result gl_onPipelineCreate(AR_device*, const AR_pipeline_desc* desc, AR_pipeline** out) {
    auto* p = new GL_PipelineState{}; p->topo=desc->topology; p->cull=desc->rasterizer.cull_mode;
    p->depth_test=desc->depth_stencil.depth_test; p->depth_write=desc->depth_stencil.depth_write; p->depth_cmp=desc->depth_stencil.depth_compare;
    GLuint prog=glCreateProgram();
    auto* vs = desc->vertex_shader ? reinterpret_cast<GL_ShaderState*>(desc->vertex_shader) : nullptr;
    auto* fs = desc->fragment_shader ? reinterpret_cast<GL_ShaderState*>(desc->fragment_shader) : nullptr;
    if(vs) glAttachShader(prog, vs->id); if(fs) glAttachShader(prog, fs->id);
    glLinkProgram(prog); GLint ok; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if(!ok) { char log[512]; glGetProgramInfoLog(prog, sizeof(log), nullptr, log); std::cerr<<"[GL] Link error: "<<log<<"\n"; glDeleteProgram(prog); delete p; return AR_result::PIPELINE_COMPILE_FAILED; }
    p->prog=prog; glGenVertexArrays(1, &p->vao);
    *out = reinterpret_cast<AR_pipeline*>(p);
    return AR_result::SUCCESS;
}
static AR_result gl_onPipelineDestroy(AR_pipeline* p) { auto* s=reinterpret_cast<GL_PipelineState*>(p); if(s->prog) glDeleteProgram(s->prog); if(s->vao) glDeleteVertexArrays(1,&s->vao); delete s; return AR_result::SUCCESS; }
static AR_result gl_onCmdBufferCreate(AR_device*, AR_command_buffer** out) { *out = reinterpret_cast<AR_command_buffer*>(new GL_CmdState{}); return AR_result::SUCCESS; }
static AR_result gl_onCmdBufferDestroy(AR_command_buffer* c) { delete reinterpret_cast<GL_CmdState*>(c); return AR_result::SUCCESS; }
static AR_result gl_onCmdBegin(AR_command_buffer* c) { auto* s=reinterpret_cast<GL_CmdState*>(c); if(s->recording) return AR_result::INVALID_OPERATION; s->recording=true; s->cmds.clear(); return AR_result::SUCCESS; }
static AR_result gl_onCmdEnd(AR_command_buffer* c) { auto* s=reinterpret_cast<GL_CmdState*>(c); if(!s->recording) return AR_result::INVALID_OPERATION; s->recording=false; return AR_result::SUCCESS; }
static AR_result gl_onCmdExecute(AR_command_buffer* cmd) {
    auto* s = reinterpret_cast<GL_CmdState*>(cmd);
    GL_PipelineState* curP = nullptr;
    for(const auto& cmd_it : s->cmds) {
        switch(cmd_it.type) {
            case GLCmd::Clear: glClearColor(cmd_it.data.clear[0], cmd_it.data.clear[1], cmd_it.data.clear[2], cmd_it.data.clear[3]); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); break;
            case GLCmd::Viewport: glViewport((int)cmd_it.data.vp[0], (int)cmd_it.data.vp[1], (int)cmd_it.data.vp[2], (int)cmd_it.data.vp[3]); break;
            case GLCmd::BindPipe:
                curP = cmd_it.data.pipe;
                if(curP) {
                    glBindVertexArray(curP->vao); glUseProgram(curP->prog);
                    if(curP->depth_test) { glEnable(GL_DEPTH_TEST); glDepthMask(curP->depth_write?GL_TRUE:GL_FALSE); glDepthFunc(gl_cmp(curP->depth_cmp)); } else glDisable(GL_DEPTH_TEST);
                    if(curP->cull==AR_cull_mode::BACK){glEnable(GL_CULL_FACE); glCullFace(GL_BACK);}
                    else if(curP->cull==AR_cull_mode::FRONT){glEnable(GL_CULL_FACE); glCullFace(GL_FRONT);}
                    else glDisable(GL_CULL_FACE);
                }
                break;
            case GLCmd::Draw: if(curP) glDrawArrays(gl_topo(curP->topo), cmd_it.data.draw.fv, cmd_it.data.draw.vc); break;
            case GLCmd::DrawIdx: if(curP) { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cmd_it.data.drawIdx.idx); glDrawElements(gl_topo(curP->topo), cmd_it.data.drawIdx.ic, GL_UNSIGNED_INT, (void*)(cmd_it.data.drawIdx.fi*sizeof(uint32_t))); } break;
            default: break;
        }
    }
    glBindVertexArray(0); glUseProgram(0);
    return AR_result::SUCCESS;
}
static void gl_onCmdClear(AR_command_buffer* c, float r, float g, float b, float a) { auto* s=reinterpret_cast<GL_CmdState*>(c); if(s->recording) { GLCommand nc; nc.type=GLCmd::Clear; nc.data.clear[0]=r; nc.data.clear[1]=g; nc.data.clear[2]=b; nc.data.clear[3]=a; s->cmds.push_back(nc); } }
static void gl_onCmdSetViewport(AR_command_buffer* c, float x, float y, float w, float h) { auto* s=reinterpret_cast<GL_CmdState*>(c); if(s->recording) { GLCommand nc; nc.type=GLCmd::Viewport; nc.data.vp[0]=x; nc.data.vp[1]=y; nc.data.vp[2]=w; nc.data.vp[3]=h; s->cmds.push_back(nc); } }
static void gl_onCmdBindPipeline(AR_command_buffer* c, AR_pipeline* p) { auto* s=reinterpret_cast<GL_CmdState*>(c); if(s->recording) { GLCommand nc; nc.type=GLCmd::BindPipe; nc.data.pipe=reinterpret_cast<GL_PipelineState*>(p); s->cmds.push_back(nc); } }
static void gl_onCmdBindMaterial(AR_command_buffer*, AR_material*) {}
static void gl_onCmdDraw(AR_command_buffer* c, uint32_t vc, uint32_t ic, uint32_t fv, uint32_t fi) { auto* s=reinterpret_cast<GL_CmdState*>(c); if(s->recording) { GLCommand nc; nc.type=GLCmd::Draw; nc.data.draw={vc,ic,fv,fi}; s->cmds.push_back(nc); } }
static void gl_onCmdDrawIndexed(AR_command_buffer* c, AR_buffer* idx, uint32_t ic, uint32_t inst, uint32_t fi, int32_t vo, uint32_t fii) {
    auto* s=reinterpret_cast<GL_CmdState*>(c); auto* b=reinterpret_cast<GL_BufferState*>(idx);
    if(s->recording && b) { GLCommand nc; nc.type=GLCmd::DrawIdx; nc.data.drawIdx={b->id, ic, inst, fi, static_cast<uint32_t>(vo), fii}; s->cmds.push_back(nc); }
}

static bool gl_is_available() {
#if ARX_GL_DESKTOP
    return glfwGetCurrentContext() != nullptr;
#else
    return emscripten_webgl_get_current_context() != 0;
#endif
}
static AR_backend_callbacks g_gl_cb = {
    gl_onContextInit, gl_onContextUninit, gl_onDeviceInit, gl_onDeviceUninit,
    gl_onDeviceStart, gl_onDeviceStop, gl_onSurfaceInit, gl_onSurfaceUninit,
    gl_onSurfaceResize, gl_onSurfacePresent, gl_onBufferCreate, gl_onBufferDestroy,
    gl_onTextureCreate, gl_onTextureDestroy, gl_onShaderCreate, gl_onShaderDestroy,
    gl_onPipelineCreate, gl_onPipelineDestroy, gl_onCmdBufferCreate, gl_onCmdBufferDestroy,
    gl_onCmdBegin, gl_onCmdEnd, gl_onCmdExecute, gl_onCmdClear, gl_onCmdSetViewport,
    gl_onCmdBindPipeline, gl_onCmdBindMaterial, gl_onCmdDraw, gl_onCmdDrawIndexed
};
static AR_backend_info g_gl_info = { AR_backend::OPENGL, "OpenGL", gl_is_available, &g_gl_cb };
AR_result AR_register_opengl_backend() { return AR_register_backend(&g_gl_info); }

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void auto_reg_gl() { AR_register_opengl_backend(); }
#elif defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void __cdecl auto_reg_gl_msvc() { AR_register_opengl_backend(); }
__declspec(allocate(".CRT$XCU")) void (*__auto_reg_gl)(void) = auto_reg_gl_msvc;
#endif
} // namespace arxrender