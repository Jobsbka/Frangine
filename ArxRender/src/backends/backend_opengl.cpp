// src/backends/backend_opengl.cpp
#include "../include/arxrender_backend.hpp"
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <any>
#include <iostream>

namespace arxrender {

// ============================================================================
// Internal State
// ============================================================================
struct GL_DeviceState { bool glad_init = false; std::mutex mtx; };
struct GL_SurfaceState { GLFWwindow* win = nullptr; uint32_t w=0, h=0; };
struct GL_BufferState { GLuint id=0; size_t size=0; AR_usage usage=AR_usage(0); };
struct GL_TextureState { GLuint id=0; uint32_t w=0, h=0; AR_format fmt=AR_format::UNDEFINED; };
struct GL_ShaderState { GLuint id=0; AR_shader_stage stage=AR_shader_stage::VERTEX; };
struct GL_PipelineState { GLuint prog=0, vao=0; AR_primitive_topology topo=AR_primitive_topology::TRIANGLE_LIST; AR_cull_mode cull=AR_cull_mode::BACK; bool depth_test=false, depth_write=true; AR_compare_op depth_cmp=AR_compare_op::LESS; };
struct GL_MaterialState { 
    std::unordered_map<std::string, GL_TextureState*> textures; 
    std::unordered_map<std::string, std::any> uniforms; 
};

enum class GLCmd { Clear, Viewport, BindPipe, BindMat, Draw, DrawIdx };
struct GLCommand {
    GLCmd type;
    union Data {
        float clear[4]; float vp[4];
        GL_PipelineState* pipe; GL_MaterialState* mat;
        struct { uint32_t vc, ic, fv, fi; } draw;
        struct { GLuint idx; uint32_t ic, inst, fi, vo; uint32_t fii; } drawIdx;
        Data() {}
    } data;
};
struct GL_CmdState { bool recording=false; std::vector<GLCommand> cmds; };

// ============================================================================
// Helpers
// ============================================================================
static void ensure_glad(GLFWwindow* win) {
    static std::once_flag f;
    std::call_once(f, [&]{
        glfwMakeContextCurrent(win);
        if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) throw std::runtime_error("GLAD failed");
        glEnable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    });
}
static GLenum gl_fmt(AR_format f) { return (f==AR_format::DEPTH32)?GL_DEPTH_COMPONENT:(f==AR_format::DEPTH24_STENCIL8)?GL_DEPTH_STENCIL:(f==AR_format::R8_UNORM)?GL_RED:GL_RGBA; }
static GLenum gl_int_fmt(AR_format f) { return (f==AR_format::DEPTH32)?GL_DEPTH_COMPONENT32F:(f==AR_format::DEPTH24_STENCIL8)?GL_DEPTH24_STENCIL8:(f==AR_format::R8_UNORM)?GL_R8:GL_RGBA8; }
static GLenum gl_topo(AR_primitive_topology t) { switch(t){case AR_primitive_topology::POINT_LIST:return GL_POINTS;case AR_primitive_topology::LINE_LIST:return GL_LINES;case AR_primitive_topology::LINE_STRIP:return GL_LINE_STRIP;case AR_primitive_topology::TRIANGLE_STRIP:return GL_TRIANGLE_STRIP;default:return GL_TRIANGLES;} }
static GLenum gl_cmp(AR_compare_op o) { switch(o){case AR_compare_op::LEQUAL:return GL_LEQUAL;case AR_compare_op::ALWAYS:return GL_ALWAYS;case AR_compare_op::EQUAL:return GL_EQUAL;case AR_compare_op::GREATER:return GL_GREATER;default:return GL_LESS;} }

// ============================================================================
// Callbacks
// ============================================================================
static AR_result gl_onCtxInit(AR_context*, const AR_context_config*) { return AR_result::SUCCESS; }
static AR_result gl_onCtxUninit(AR_context*) { return AR_result::SUCCESS; }

static AR_result gl_onDevCreate(AR_context*, const AR_device_config*, AR_device** out) {
    *out = new(std::nothrow) AR_device{};
    if(*out) (*out)->p_impl->backend_data = new GL_DeviceState{};
    return *out ? AR_result::SUCCESS : AR_result::OUT_OF_MEMORY;
}
static void gl_onDevDestroy(AR_device* d) { if(d && d->p_impl) delete static_cast<GL_DeviceState*>(d->p_impl->backend_data); delete d; }

static AR_result gl_onSurfCreate(AR_context*, const AR_surface_config* cfg, AR_surface** out) {
    *out = new(std::nothrow) AR_surface{};
    if(*out) {
        (*out)->p_impl->backend_data = new GL_SurfaceState{static_cast<GLFWwindow*>(cfg->native_window_handle), cfg->width, cfg->height};
    }
    return *out ? AR_result::SUCCESS : AR_result::OUT_OF_MEMORY;
}
static void gl_onSurfDestroy(AR_surface* s) { if(s && s->p_impl) delete static_cast<GL_SurfaceState*>(s->p_impl->backend_data); delete s; }
static AR_result gl_onSurfResize(AR_surface* s, uint32_t w, uint32_t h) {
    auto* st = static_cast<GL_SurfaceState*>(s->p_impl->backend_data); st->w=w; st->h=h; return AR_result::SUCCESS;
}
static AR_result gl_onSurfPresent(AR_surface* s, AR_command_buffer*) {
    auto* st = static_cast<GL_SurfaceState*>(s->p_impl->backend_data);
    if(st && st->win) { glfwMakeContextCurrent(st->win); glfwSwapBuffers(st->win); }
    return AR_result::SUCCESS;
}

static AR_result gl_onBufCreate(AR_device*, const AR_buffer_desc* desc, AR_buffer** out) {
    GLuint target = (desc->usage & AR_usage::INDEX_BUFFER) ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    GLuint id=0; glGenBuffers(1, &id); glBindBuffer(target, id); glBufferData(target, desc->size, desc->initial_data, GL_STATIC_DRAW); glBindBuffer(target, 0);
    auto* b = new(std::nothrow) GL_BufferState{id, desc->size, desc->usage}; *out = reinterpret_cast<AR_buffer*>(b); return AR_result::SUCCESS;
}
static void gl_onBufDestroy(AR_buffer* b) { auto* s=reinterpret_cast<GL_BufferState*>(b); if(s&&s->id) glDeleteBuffers(1, &s->id); delete s; }

static AR_result gl_onTexCreate(AR_device*, const AR_texture_desc* desc, AR_texture** out) {
    GLuint id=0; glGenTextures(1, &id); glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, gl_int_fmt(desc->format), desc->width, desc->height, 0, gl_fmt(desc->format), GL_UNSIGNED_BYTE, desc->initial_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, desc->mip_levels>1?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if(desc->initial_data && desc->mip_levels>1) glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    auto* t = new(std::nothrow) GL_TextureState{id, desc->width, desc->height, desc->format}; *out = reinterpret_cast<AR_texture*>(t); return AR_result::SUCCESS;
}
static void gl_onTexDestroy(AR_texture* t) { auto* s=reinterpret_cast<GL_TextureState*>(t); if(s&&s->id) glDeleteTextures(1, &s->id); delete s; }

static AR_result gl_onShadCreate(AR_device*, AR_shader_stage stage, const void* code, size_t size, const char*, AR_shader** out) {
    GLenum gl_stage = (stage==AR_shader_stage::VERTEX)?GL_VERTEX_SHADER:GL_FRAGMENT_SHADER;
    GLuint id=glCreateShader(gl_stage); const char* src=static_cast<const char*>(code); glShaderSource(id, 1, &src, nullptr); glCompileShader(id);
    GLint ok; glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if(!ok) { char log[512]; glGetShaderInfoLog(id, sizeof(log), nullptr, log); std::cerr<<"[GL] Shader compile: "<<log<<"\n"; glDeleteShader(id); return AR_result::PIPELINE_COMPILE_FAILED; }
    *out = reinterpret_cast<AR_shader*>(new GL_ShaderState{id, stage}); return AR_result::SUCCESS;
}
static void gl_onShadDestroy(AR_shader* s) { auto* st=reinterpret_cast<GL_ShaderState*>(s); if(st&&st->id) glDeleteShader(st->id); delete st; }

static AR_result gl_onPipeCreate(AR_device*, const AR_pipeline_desc* desc, AR_pipeline** out) {
    GLuint prog=glCreateProgram();
    auto* vs = reinterpret_cast<GL_ShaderState*>(desc->vertex_shader);
    auto* fs = reinterpret_cast<GL_ShaderState*>(desc->fragment_shader);
    if(vs) glAttachShader(prog, vs->id); if(fs) glAttachShader(prog, fs->id);
    glLinkProgram(prog); GLint ok; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if(!ok) { char log[512]; glGetProgramInfoLog(prog, sizeof(log), nullptr, log); std::cerr<<"[GL] Link: "<<log<<"\n"; glDeleteProgram(prog); return AR_result::PIPELINE_COMPILE_FAILED; }
    GLuint vao=0; glGenVertexArrays(1, &vao);
    auto* p = new GL_PipelineState{prog, vao, desc->topology, desc->rasterizer.cull_mode, desc->depth_stencil.depth_test, desc->depth_stencil.depth_write, desc->depth_stencil.depth_compare};
    *out = reinterpret_cast<AR_pipeline*>(p); return AR_result::SUCCESS;
}
static void gl_onPipeDestroy(AR_pipeline* p) { auto* s=reinterpret_cast<GL_PipelineState*>(p); if(s){if(s->prog) glDeleteProgram(s->prog); if(s->vao) glDeleteVertexArrays(1, &s->vao);} delete s; }

static AR_result gl_onCmdCreate(AR_device*, AR_command_buffer** out) { *out = reinterpret_cast<AR_command_buffer*>(new GL_CmdState{}); return AR_result::SUCCESS; }
static void gl_onCmdDestroy(AR_command_buffer* c) { delete reinterpret_cast<GL_CmdState*>(c); }
static AR_result gl_onCmdBegin(AR_command_buffer* c) { auto* s=reinterpret_cast<GL_CmdState*>(c); if(s->recording) return AR_result::INVALID_OPERATION; s->recording=true; s->cmds.clear(); return AR_result::SUCCESS; }
static AR_result gl_onCmdEnd(AR_command_buffer* c) { auto* s=reinterpret_cast<GL_CmdState*>(c); if(!s->recording) return AR_result::INVALID_OPERATION; s->recording=false; return AR_result::SUCCESS; }

static AR_result gl_onCmdExec(AR_command_buffer* cmd) {
    auto* s = reinterpret_cast<GL_CmdState*>(cmd);
    GL_PipelineState* curP = nullptr; GL_MaterialState* curM = nullptr;
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
            case GLCmd::BindMat:
                curM = cmd_it.data.mat;
                if(curM && curP) {
                    int texUnit=0;
                    for(auto& [n, t] : curM->textures) { if(t) { glActiveTexture(GL_TEXTURE0+texUnit); glBindTexture(GL_TEXTURE_2D, t->id); GLint loc=glGetUniformLocation(curP->prog, n.c_str()); if(loc!=-1) glUniform1i(loc, texUnit); ++texUnit; } }
                    for(auto& [n, v] : curM->uniforms) {
                        GLint loc = glGetUniformLocation(curP->prog, n.c_str()); if(loc==-1) continue;
                        if(v.type()==typeid(float)) glUniform1f(loc, std::any_cast<float>(v));
                        else if(v.type()==typeid(std::array<float,3>)) {auto a=std::any_cast<std::array<float,3>>(v); glUniform3fv(loc,1,a.data());}
                        else if(v.type()==typeid(std::array<float,4>)) {auto a=std::any_cast<std::array<float,4>>(v); glUniform4fv(loc,1,a.data());}
                        else if(v.type()==typeid(std::array<float,16>)) {auto a=std::any_cast<std::array<float,16>>(v); glUniformMatrix4fv(loc,1,GL_FALSE,a.data());}
                    }
                }
                break;
            case GLCmd::Draw: if(curP) glDrawArrays(gl_topo(curP->topo), cmd_it.data.draw.fv, cmd_it.data.draw.vc); break;
            case GLCmd::DrawIdx: if(curP) { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cmd_it.data.drawIdx.idx); glDrawElements(gl_topo(curP->topo), cmd_it.data.drawIdx.ic, GL_UNSIGNED_INT, (void*)(cmd_it.data.drawIdx.fi*sizeof(uint32_t))); } break;
        }
    }
    glBindVertexArray(0); glUseProgram(0); glBindBuffer(GL_ARRAY_BUFFER,0); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
    return AR_result::SUCCESS;
}

static void gl_onCmdClear(AR_command_buffer* c, float r, float g, float b, float a) { auto* s=reinterpret_cast<GL_CmdState*>(c); if(s->recording) s->cmds.push_back({GLCmd::Clear, {{r,g,b,a}}}); }
static void gl_onCmdVP(AR_command_buffer* c, float x, float y, float w, float h) { auto* s=reinterpret_cast<GL_CmdState*>(c); if(s->recording) s->cmds.push_back({GLCmd::Viewport, {{x,y,w,h}}}); }
static void gl_onCmdBP(AR_command_buffer* c, AR_pipeline* p) { auto* s=reinterpret_cast<GL_CmdState*>(c); if(s->recording) s->cmds.push_back({GLCmd::BindPipe, {reinterpret_cast<GL_PipelineState*>(p)}}); }
static void gl_onCmdBM(AR_command_buffer* c, AR_material* m) { auto* s=reinterpret_cast<GL_CmdState*>(c); if(s->recording) s->cmds.push_back({GLCmd::BindMat, {reinterpret_cast<GL_MaterialState*>(m)}}); }
static void gl_onCmdDraw(AR_command_buffer* c, uint32_t vc, uint32_t ic, uint32_t fv, uint32_t fi) { auto* s=reinterpret_cast<GL_CmdState*>(c); if(s->recording) s->cmds.push_back({GLCmd::Draw, {{vc,ic,fv,fi}}}); }
static void gl_onCmdDI(AR_command_buffer* c, AR_buffer* idx, uint32_t ic, uint32_t inst, uint32_t fi, int32_t vo, uint32_t fii) {
    auto* s=reinterpret_cast<GL_CmdState*>(c); auto* b=reinterpret_cast<GL_BufferState*>(idx);
    if(s->recording && b) s->cmds.push_back({GLCmd::DrawIdx, {{b->id, ic, inst, fi, (uint32_t)vo, fii}}});
}

// ============================================================================
// Registration
// ============================================================================
static AS_bool32 gl_is_avail() { return glfwGetCurrentContext()!=nullptr; }
static AR_backend_callbacks g_gl_cb = {
    gl_onCtxInit, gl_onCtxUninit, gl_onDevCreate, gl_onDevDestroy, gl_onSurfCreate, gl_onSurfDestroy, gl_onSurfResize, gl_onSurfPresent,
    gl_onBufCreate, gl_onBufDestroy, gl_onTexCreate, gl_onTexDestroy, gl_onShadCreate, gl_onShadDestroy, gl_onPipeCreate, gl_onPipeDestroy,
    gl_onCmdCreate, gl_onCmdDestroy, gl_onCmdBegin, gl_onCmdEnd, gl_onCmdExec, gl_onCmdClear, gl_onCmdVP, gl_onCmdBP, gl_onCmdBM, gl_onCmdDraw, gl_onCmdDI
};
static AR_backend_info g_gl_info = { AR_backend::OPENGL, "OpenGL", gl_is_avail, &g_gl_cb };
AR_result AR_register_opengl_backend() { return AR_register_backend(&g_gl_info); }

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void auto_reg_gl() { AR_register_opengl_backend(); }
#elif defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void __cdecl auto_reg_gl_msvc() { AR_register_opengl_backend(); }
__declspec(allocate(".CRT$XCU")) void (*__auto_reg_gl)(void) = auto_reg_gl_msvc;
#endif
} // namespace arxrender