// src/backends/backend_webgl.cpp
#include "../include/arxrender_backend.hpp"
#include <GLES3/gl3.h>
#include <emscripten/html5.h>
#include <emscripten/webgl.h>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace arxrender {

// ============================================================================
// Internal state structures
// ============================================================================
struct WGL_State_Context {
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE defaultContext = 0;
    std::mutex mutex;
};

struct WGL_State_Device {
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = 0;
    bool ownsContext = false;
    std::mutex mutex;
};

struct WGL_State_Surface {
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = 0;
    uint32_t width = 0, height = 0;
    bool vsync = true;
};

struct WGL_State_Buffer {
    GLuint bufferId = 0;
    GLenum target = GL_ARRAY_BUFFER;
    size_t size = 0;
    AR_usage usage = AR_usage(0);
};

struct WGL_State_Texture {
    GLuint textureId = 0;
    uint32_t width = 1, height = 1;
    AR_format format = AR_format::UNDEFINED;
    GLint internalFormat = GL_RGBA8;
    GLenum formatEnum = GL_RGBA;
    GLenum type = GL_UNSIGNED_BYTE;
};

struct WGL_State_Shader {
    GLuint shaderId = 0;
    AR_shader_stage stage = AR_shader_stage::VERTEX;
};

struct WGL_State_Pipeline {
    GLuint programId = 0;
    AR_primitive_topology topology = AR_primitive_topology::TRIANGLE_LIST;
    AR_blend_state blend;
    AR_depth_stencil_state depthStencil;
    AR_rasterizer_state rasterizer;
    bool depthTestEnabled = false;
    bool cullFaceEnabled = false;
};

struct WGL_State_Material {
    WGL_State_Pipeline* pipeline = nullptr;
    std::unordered_map<std::string, WGL_State_Texture*> textures;
    std::unordered_map<std::string, std::any> uniforms;
    std::unordered_map<std::string, GLint> uniformLocations;
};

struct WGL_State_CmdBuffer {
    bool recording = false;
    struct Command {
        enum Type { Clear, Viewport, Scissor, BindPipeline, BindMaterial, Draw, DrawIndexed } type;
        union Data {
            struct { float r, g, b, a; } clear;
            struct { float x, y, w, h; } viewport;
            struct { uint32_t x, y, w, h; } scissor;
            WGL_State_Pipeline* pipeline;
            WGL_State_Material* material;
            struct { uint32_t vc, ic, fv, fi; } draw;
            struct { GLuint indexBuffer; uint32_t ic, inst, fi; int32_t vo; uint32_t fii; } drawIdx;
            Data() {}
        } data;
    };
    std::vector<Command> commands;
};

// ============================================================================
// Helper functions
// ============================================================================
static GLenum gl_format_webgl(AR_format fmt) {
    switch (fmt) {
        case AR_format::R8_UNORM: return GL_RED;
        case AR_format::RG8_UNORM: return GL_RG;
        case AR_format::RGB8_UNORM: return GL_RGB;
        case AR_format::RGBA8_UNORM: return GL_RGBA;
        case AR_format::R16_UNORM: return GL_RED;
        case AR_format::RG16_UNORM: return GL_RG;
        case AR_format::RGBA16_UNORM: return GL_RGBA;
        case AR_format::R32_FLOAT: return GL_RED;
        case AR_format::RG32_FLOAT: return GL_RG;
        case AR_format::RGB32_FLOAT: return GL_RGB;
        case AR_format::RGBA32_FLOAT: return GL_RGBA;
        case AR_format::R32_UINT: return GL_RED_INTEGER;
        case AR_format::RG32_UINT: return GL_RG_INTEGER;
        case AR_format::RGBA32_UINT: return GL_RGBA_INTEGER;
        case AR_format::DEPTH32: return GL_DEPTH_COMPONENT;
        case AR_format::DEPTH24_STENCIL8: return GL_DEPTH_STENCIL;
        default: return GL_RGBA;
    }
}

static GLenum gl_internal_format_webgl(AR_format fmt) {
    switch (fmt) {
        case AR_format::R8_UNORM: return GL_R8;
        case AR_format::RG8_UNORM: return GL_RG8;
        case AR_format::RGB8_UNORM: return GL_RGB8;
        case AR_format::RGBA8_UNORM: return GL_RGBA8;
        case AR_format::R16_UNORM: return GL_R16;
        case AR_format::RG16_UNORM: return GL_RG16;
        case AR_format::RGBA16_UNORM: return GL_RGBA16;
        case AR_format::R32_FLOAT: return GL_R32F;
        case AR_format::RG32_FLOAT: return GL_RG32F;
        case AR_format::RGB32_FLOAT: return GL_RGB32F;
        case AR_format::RGBA32_FLOAT: return GL_RGBA32F;
        case AR_format::R32_UINT: return GL_R32UI;
        case AR_format::RG32_UINT: return GL_RG32UI;
        case AR_format::RGBA32_UINT: return GL_RGBA32UI;
        case AR_format::DEPTH32: return GL_DEPTH_COMPONENT32F;
        case AR_format::DEPTH24_STENCIL8: return GL_DEPTH24_STENCIL8;
        default: return GL_RGBA8;
    }
}

static GLenum gl_topology_webgl(AR_primitive_topology t) {
    switch (t) {
        case AR_primitive_topology::POINT_LIST: return GL_POINTS;
        case AR_primitive_topology::LINE_LIST: return GL_LINES;
        case AR_primitive_topology::LINE_STRIP: return GL_LINE_STRIP;
        case AR_primitive_topology::TRIANGLE_LIST: return GL_TRIANGLES;
        case AR_primitive_topology::TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
        case AR_primitive_topology::TRIANGLE_FAN: return GL_TRIANGLE_FAN;
        default: return GL_TRIANGLES;
    }
}

static GLenum gl_blend_factor_webgl(AR_blend_factor f) {
    switch (f) {
        case AR_blend_factor::ZERO: return GL_ZERO;
        case AR_blend_factor::ONE: return GL_ONE;
        case AR_blend_factor::SRC_COLOR: return GL_SRC_COLOR;
        case AR_blend_factor::ONE_MINUS_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR;
        case AR_blend_factor::DST_COLOR: return GL_DST_COLOR;
        case AR_blend_factor::ONE_MINUS_DST_COLOR: return GL_ONE_MINUS_DST_COLOR;
        case AR_blend_factor::SRC_ALPHA: return GL_SRC_ALPHA;
        case AR_blend_factor::ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
        case AR_blend_factor::DST_ALPHA: return GL_DST_ALPHA;
        case AR_blend_factor::ONE_MINUS_DST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
        default: return GL_ONE;
    }
}

static GLenum gl_blend_op_webgl(AR_blend_op op) {
    switch (op) {
        case AR_blend_op::ADD: return GL_FUNC_ADD;
        case AR_blend_op::SUBTRACT: return GL_FUNC_SUBTRACT;
        case AR_blend_op::REVERSE_SUBTRACT: return GL_FUNC_REVERSE_SUBTRACT;
        case AR_blend_op::MIN: return GL_MIN;
        case AR_blend_op::MAX: return GL_MAX;
        default: return GL_FUNC_ADD;
    }
}

static GLenum gl_compare_op_webgl(AR_compare_op op) {
    switch (op) {
        case AR_compare_op::NEVER: return GL_NEVER;
        case AR_compare_op::LESS: return GL_LESS;
        case AR_compare_op::EQUAL: return GL_EQUAL;
        case AR_compare_op::LESS_EQUAL: return GL_LEQUAL;
        case AR_compare_op::GREATER: return GL_GREATER;
        case AR_compare_op::NOT_EQUAL: return GL_NOTEQUAL;
        case AR_compare_op::GREATER_EQUAL: return GL_GEQUAL;
        case AR_compare_op::ALWAYS: return GL_ALWAYS;
        default: return GL_LESS;
    }
}

static GLenum gl_cull_mode_webgl(AR_cull_mode mode) {
    switch (mode) {
        case AR_cull_mode::NONE: return GL_NONE;
        case AR_cull_mode::FRONT: return GL_FRONT;
        case AR_cull_mode::BACK: return GL_BACK;
        default: return GL_BACK;
    }
}

static GLenum gl_front_face_webgl(AR_front_face face) {
    return (face == AR_front_face::CW) ? GL_CW : GL_CCW;
}

static GLenum gl_polygon_mode_webgl(AR_polygon_mode mode) {
    // WebGL only supports GL_FILL
    (void)mode;
    return GL_FILL;
}

static void check_gl_error(const char* location) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "[WebGL] OpenGL error at " << location << ": " << err << std::endl;
    }
}

// ============================================================================
// Backend callbacks implementation
// ============================================================================

// --- Context callbacks ---
static AR_result webgl_onContextInit(AR_context*, const AR_context_config*) {
    // WebGL context is managed by browser/Emscripten
    return AR_result::SUCCESS;
}

static AR_result webgl_onContextUninit(AR_context*) {
    return AR_result::SUCCESS;
}

static AR_result webgl_onContextEnumerateDevices(AR_context*, AR_device_type, 
    AR_enumerate_devices_callback, void*) {
    return AR_result::NOT_IMPLEMENTED;
}

static AR_result webgl_onContextGetDeviceInfo(AR_context*, AR_device_type, 
    const void*, AR_device_info*) {
    return AR_result::NOT_IMPLEMENTED;
}

// --- Device callbacks ---
static AR_result webgl_onDeviceInit(AR_context*, AR_device_type, const void*, 
    const AR_device_config* config, AR_device* device) {
    
    if (!config || !device) return AR_result::INVALID_ARGS;
    
    auto* devState = new WGL_State_Device{};
    if (!devState) return AR_result::OUT_OF_MEMORY;
    
    // Get or create WebGL context
    devState->context = emscripten_webgl_get_current_context();
    
    if (!devState->context) {
        // Create new context
        EmscriptenWebGLContextAttributes attrs;
        emscripten_webgl_init_context_attributes(&attrs);
        
        attrs.enableExtensionsByDefault = 1;
        attrs.explicitSwapControl = 0;
        attrs.alpha = 1;
        attrs.depth = config->debug_layer ? 1 : 0;
        attrs.stencil = 0;
        attrs.antialias = 1;
        attrs.premultipliedAlpha = 1;
        attrs.preserveDrawingBuffer = 0;
        attrs.powerPreference = EMSCRIPTEN_WEBGL_POWER_PREFERENCE_DEFAULT;
        attrs.failIfMajorPerformanceCaveat = 0;
        attrs.majorVersion = 2;  // WebGL 2.0
        attrs.minorVersion = 0;
        
        devState->context = emscripten_webgl_create_context("#canvas", &attrs);
        if (!devState->context) {
            delete devState;
            return AR_result::NO_BACKEND;
        }
        devState->ownsContext = true;
        
        // Make context current
        emscripten_webgl_make_context_current(devState->context);
    }
    
    // Initialize GL state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    
    device->p_impl->backend_data = static_cast<void*>(devState);
    return AR_result::SUCCESS;
}

static AR_result webgl_onDeviceUninit(AR_device* device) {
    if (!device || !device->p_impl) return AR_result::INVALID_ARGS;
    
    auto* devState = static_cast<WGL_State_Device*>(device->p_impl->backend_data);
    if (!devState) return AR_result::SUCCESS;
    
    // Destroy context if we own it
    if (devState->ownsContext && devState->context) {
        emscripten_webgl_destroy_context(devState->context);
    }
    
    delete devState;
    device->p_impl->backend_data = nullptr;
    
    return AR_result::SUCCESS;
}

static AR_result webgl_onDeviceStart(AR_device*) { return AR_result::SUCCESS; }
static AR_result webgl_onDeviceStop(AR_device*) { return AR_result::SUCCESS; }

// --- Surface callbacks ---
static AR_result webgl_onSurfaceInit(AR_context*, const AR_surface_config* config, AR_surface* surface) {
    if (!config || !surface) return AR_result::INVALID_ARGS;
    
    auto* surfState = new WGL_State_Surface{};
    if (!surfState) return AR_result::OUT_OF_MEMORY;
    
    surfState->context = emscripten_webgl_get_current_context();
    surfState->width = config->width;
    surfState->height = config->height;
    surfState->vsync = config->vsync;
    
    // Set canvas size
    EMSCRIPTEN_RESULT res = emscripten_set_canvas_element_size("#canvas", 
        static_cast<int>(config->width), static_cast<int>(config->height));
    if (res != EMSCRIPTEN_RESULT_SUCCESS) {
        delete surfState;
        return AR_result::ERROR_GENERIC;
    }
    
    // Set viewport
    glViewport(0, 0, config->width, config->height);
    
    surface->p_impl->backend_data = static_cast<void*>(surfState);
    return AR_result::SUCCESS;
}

static AR_result webgl_onSurfaceUninit(AR_surface* surface) {
    if (!surface || !surface->p_impl) return AR_result::INVALID_ARGS;
    
    auto* surfState = static_cast<WGL_State_Surface*>(surface->p_impl->backend_data);
    if (!surfState) return AR_result::SUCCESS;
    
    delete surfState;
    surface->p_impl->backend_data = nullptr;
    
    return AR_result::SUCCESS;
}

static AR_result webgl_onSurfaceResize(AR_surface* surface, uint32_t width, uint32_t height) {
    if (!surface || !surface->p_impl) return AR_result::INVALID_ARGS;
    
    auto* surfState = static_cast<WGL_State_Surface*>(surface->p_impl->backend_data);
    if (!surfState) return AR_result::INVALID_OPERATION;
    
    surfState->width = width;
    surfState->height = height;
    
    // Update canvas and viewport
    emscripten_set_canvas_element_size("#canvas", width, height);
    glViewport(0, 0, width, height);
    
    return AR_result::SUCCESS;
}

static AR_result webgl_onSurfacePresent(AR_surface*, AR_command_buffer*) {
    // WebGL presents automatically via requestAnimationFrame
    // This is called to signal frame completion
    return AR_result::SUCCESS;
}

// --- Resource callbacks ---
static AR_result webgl_onBufferCreate(AR_device*, const AR_buffer_desc* desc, AR_buffer** out) {
    if (!desc || !out) return AR_result::INVALID_ARGS;
    
    auto* bufState = new WGL_State_Buffer{};
    if (!bufState) return AR_result::OUT_OF_MEMORY;
    
    bufState->size = desc->size;
    bufState->usage = desc->usage;
    
    // Determine target
    if (desc->usage & AR_usage::INDEX_BUFFER) {
        bufState->target = GL_ELEMENT_ARRAY_BUFFER;
    } else {
        bufState->target = GL_ARRAY_BUFFER;
    }
    
    // Determine usage hint
    GLenum usageHint = GL_STATIC_DRAW;
    if (desc->host_visible) {
        usageHint = GL_DYNAMIC_DRAW;
    }
    
    // Create and upload buffer
    glGenBuffers(1, &bufState->bufferId);
    glBindBuffer(bufState->target, bufState->bufferId);
    glBufferData(bufState->target, desc->size, desc->initial_data, usageHint);
    glBindBuffer(bufState->target, 0);
    
    check_gl_error("webgl_onBufferCreate");
    
    *out = reinterpret_cast<AR_buffer*>(bufState);
    return AR_result::SUCCESS;
}

static void webgl_onBufferDestroy(AR_buffer* buffer) {
    if (!buffer) return;
    
    auto* bufState = reinterpret_cast<WGL_State_Buffer*>(buffer);
    if (bufState->bufferId) {
        glDeleteBuffers(1, &bufState->bufferId);
    }
    delete bufState;
}

static AR_result webgl_onTextureCreate(AR_device*, const AR_texture_desc* desc, AR_texture** out) {
    if (!desc || !out) return AR_result::INVALID_ARGS;
    
    auto* texState = new WGL_State_Texture{};
    if (!texState) return AR_result::OUT_OF_MEMORY;
    
    texState->width = desc->width;
    texState->height = desc->height;
    texState->format = desc->format;
    texState->formatEnum = gl_format_webgl(desc->format);
    texState->internalFormat = gl_internal_format_webgl(desc->format);
    texState->type = GL_UNSIGNED_BYTE;  // Default, could be extended
    
    // Create texture
    glGenTextures(1, &texState->textureId);
    glBindTexture(GL_TEXTURE_2D, texState->textureId);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
        desc->mip_levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Upload data
    if (desc->initial_data) {
        glTexImage2D(GL_TEXTURE_2D, 0, texState->internalFormat, 
            desc->width, desc->height, 0, 
            texState->formatEnum, texState->type, desc->initial_data);
        
        // Generate mipmaps if requested
        if (desc->generate_mips_on_create && desc->mip_levels > 1) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    } else {
        // Allocate storage without data
        glTexImage2D(GL_TEXTURE_2D, 0, texState->internalFormat,
            desc->width, desc->height, 0,
            texState->formatEnum, texState->type, nullptr);
    }
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    check_gl_error("webgl_onTextureCreate");
    
    *out = reinterpret_cast<AR_texture*>(texState);
    return AR_result::SUCCESS;
}

static void webgl_onTextureDestroy(AR_texture* texture) {
    if (!texture) return;
    
    auto* texState = reinterpret_cast<WGL_State_Texture*>(texture);
    if (texState->textureId) {
        glDeleteTextures(1, &texState->textureId);
    }
    delete texState;
}

static AR_result webgl_onShaderCreate(AR_device*, AR_shader_stage stage, 
    const void* code, size_t, const char*, AR_shader** out) {
    
    if (!code || !out) return AR_result::INVALID_ARGS;
    
    auto* shState = new WGL_State_Shader{};
    if (!shState) return AR_result::OUT_OF_MEMORY;
    
    shState->stage = stage;
    
    GLenum shaderType = (stage == AR_shader_stage::VERTEX) ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
    
    shState->shaderId = glCreateShader(shaderType);
    if (!shState->shaderId) {
        delete shState;
        return AR_result::ERROR_GENERIC;
    }
    
    const char* src = static_cast<const char*>(code);
    glShaderSource(shState->shaderId, 1, &src, nullptr);
    glCompileShader(shState->shaderId);
    
    // Check compilation
    GLint success;
    glGetShaderiv(shState->shaderId, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shState->shaderId, sizeof(log), nullptr, log);
        std::cerr << "[WebGL] Shader compile error: " << log << std::endl;
        glDeleteShader(shState->shaderId);
        delete shState;
        return AR_result::PIPELINE_COMPILE_FAILED;
    }
    
    *out = reinterpret_cast<AR_shader*>(shState);
    return AR_result::SUCCESS;
}

static void webgl_onShaderDestroy(AR_shader* shader) {
    if (!shader) return;
    
    auto* shState = reinterpret_cast<WGL_State_Shader*>(shader);
    if (shState->shaderId) {
        glDeleteShader(shState->shaderId);
    }
    delete shState;
}

static AR_result webgl_onPipelineCreate(AR_device*, const AR_pipeline_desc* desc, AR_pipeline** out) {
    if (!desc || !out) return AR_result::INVALID_ARGS;
    
    auto* pipeState = new WGL_State_Pipeline{};
    if (!pipeState) return AR_result::OUT_OF_MEMORY;
    
    pipeState->topology = desc->topology;
    pipeState->blend = desc->blend;
    pipeState->depthStencil = desc->depth_stencil;
    pipeState->rasterizer = desc->rasterizer;
    
    // Create program
    pipeState->programId = glCreateProgram();
    if (!pipeState->programId) {
        delete pipeState;
        return AR_result::ERROR_GENERIC;
    }
    
    // Attach shaders
    if (desc->vertex_shader) {
        auto* vs = reinterpret_cast<WGL_State_Shader*>(desc->vertex_shader);
        glAttachShader(pipeState->programId, vs->shaderId);
    }
    if (desc->fragment_shader) {
        auto* fs = reinterpret_cast<WGL_State_Shader*>(desc->fragment_shader);
        glAttachShader(pipeState->programId, fs->shaderId);
    }
    
    // Link program
    glLinkProgram(pipeState->programId);
    
    // Check linking
    GLint success;
    glGetProgramiv(pipeState->programId, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetProgramInfoLog(pipeState->programId, sizeof(log), nullptr, log);
        std::cerr << "[WebGL] Program link error: " << log << std::endl;
        glDeleteProgram(pipeState->programId);
        delete pipeState;
        return AR_result::PIPELINE_COMPILE_FAILED;
    }
    
    // Cache uniform locations
    glUseProgram(pipeState->programId);
    GLint numUniforms;
    glGetProgramiv(pipeState->programId, GL_ACTIVE_UNIFORMS, &numUniforms);
    
    for (GLint i = 0; i < numUniforms; ++i) {
        char name[256];
        GLsizei length;
        GLint size;
        GLenum type;
        glGetActiveUniform(pipeState->programId, i, sizeof(name), &length, &size, &type, name);
        
        GLint loc = glGetUniformLocation(pipeState->programId, name);
        if (loc != -1) {
            // Store for material binding
        }
    }
    glUseProgram(0);
    
    *out = reinterpret_cast<AR_pipeline*>(pipeState);
    return AR_result::SUCCESS;
}

static void webgl_onPipelineDestroy(AR_pipeline* pipeline) {
    if (!pipeline) return;
    
    auto* pipeState = reinterpret_cast<WGL_State_Pipeline*>(pipeline);
    if (pipeState->programId) {
        glDeleteProgram(pipeState->programId);
    }
    delete pipeState;
}

// --- Command buffer callbacks ---
static AR_result webgl_onCmdBufferCreate(AR_device*, AR_command_buffer** out) {
    if (!out) return AR_result::INVALID_ARGS;
    
    auto* cmdState = new WGL_State_CmdBuffer{};
    if (!cmdState) return AR_result::OUT_OF_MEMORY;
    
    *out = reinterpret_cast<AR_command_buffer*>(cmdState);
    return AR_result::SUCCESS;
}

static void webgl_onCmdBufferDestroy(AR_command_buffer* cmd) {
    if (!cmd) return;
    
    auto* cmdState = reinterpret_cast<WGL_State_CmdBuffer*>(cmd);
    delete cmdState;
}

static AR_result webgl_onCmdBegin(AR_command_buffer* cmd) {
    if (!cmd) return AR_result::INVALID_ARGS;
    
    auto* cmdState = reinterpret_cast<WGL_State_CmdBuffer*>(cmd);
    if (cmdState->recording) return AR_result::INVALID_OPERATION;
    
    cmdState->commands.clear();
    cmdState->recording = true;
    
    return AR_result::SUCCESS;
}

static AR_result webgl_onCmdEnd(AR_command_buffer* cmd) {
    if (!cmd) return AR_result::INVALID_ARGS;
    
    auto* cmdState = reinterpret_cast<WGL_State_CmdBuffer*>(cmd);
    if (!cmdState->recording) return AR_result::INVALID_OPERATION;
    
    cmdState->recording = false;
    
    return AR_result::SUCCESS;
}

static AR_result webgl_onCmdExecute(AR_command_buffer* cmd) {
    if (!cmd) return AR_result::INVALID_ARGS;
    
    auto* cmdState = reinterpret_cast<WGL_State_CmdBuffer*>(cmd);
    
    // Execute deferred commands
    WGL_State_Pipeline* currentPipeline = nullptr;
    WGL_State_Material* currentMaterial = nullptr;
    
    for (const auto& command : cmdState->commands) {
        switch (command.type) {
            case WGL_State_CmdBuffer::Command::Type::Clear:
                glClearColor(command.data.clear.r, command.data.clear.g, 
                           command.data.clear.b, command.data.clear.a);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                break;
                
            case WGL_State_CmdBuffer::Command::Type::Viewport:
                glViewport(static_cast<GLint>(command.data.viewport.x),
                          static_cast<GLint>(command.data.viewport.y),
                          static_cast<GLsizei>(command.data.viewport.w),
                          static_cast<GLsizei>(command.data.viewport.h));
                break;
                
            case WGL_State_CmdBuffer::Command::Type::Scissor:
                glEnable(GL_SCISSOR_TEST);
                glScissor(command.data.scissor.x, command.data.scissor.y,
                         command.data.scissor.w, command.data.scissor.h);
                break;
                
            case WGL_State_CmdBuffer::Command::Type::BindPipeline:
                currentPipeline = command.data.pipeline;
                if (currentPipeline) {
                    glUseProgram(currentPipeline->programId);
                    
                    // Apply rasterizer state
                    if (currentPipeline->rasterizer.cull_mode != AR_cull_mode::NONE) {
                        glEnable(GL_CULL_FACE);
                        glCullFace(gl_cull_mode_webgl(currentPipeline->rasterizer.cull_mode));
                        glFrontFace(gl_front_face_webgl(currentPipeline->rasterizer.front_face));
                    } else {
                        glDisable(GL_CULL_FACE);
                    }
                    
                    // Apply depth state
                    if (currentPipeline->depthStencil.depth_test) {
                        glEnable(GL_DEPTH_TEST);
                        glDepthMask(currentPipeline->depthStencil.depth_write ? GL_TRUE : GL_FALSE);
                        glDepthFunc(gl_compare_op_webgl(currentPipeline->depthStencil.depth_compare));
                    } else {
                        glDisable(GL_DEPTH_TEST);
                    }
                    
                    // Apply blend state
                    if (currentPipeline->blend.enabled) {
                        glEnable(GL_BLEND);
                        glBlendFunc(gl_blend_factor_webgl(currentPipeline->blend.src_color),
                                   gl_blend_factor_webgl(currentPipeline->blend.dst_color));
                        glBlendEquation(gl_blend_op_webgl(currentPipeline->blend.color_op));
                    } else {
                        glDisable(GL_BLEND);
                    }
                }
                break;
                
            case WGL_State_CmdBuffer::Command::Type::BindMaterial:
                currentMaterial = command.data.material;
                if (currentMaterial && currentPipeline) {
                    // Bind textures
                    GLint texUnit = 0;
                    for (const auto& [name, tex] : currentMaterial->textures) {
                        if (tex && tex->textureId) {
                            glActiveTexture(GL_TEXTURE0 + texUnit);
                            glBindTexture(GL_TEXTURE_2D, tex->textureId);
                            
                            GLint loc = glGetUniformLocation(currentPipeline->programId, name.c_str());
                            if (loc != -1) {
                                glUniform1i(loc, texUnit);
                            }
                            ++texUnit;
                        }
                    }
                    
                    // Set uniforms
                    for (const auto& [name, value] : currentMaterial->uniforms) {
                        GLint loc = glGetUniformLocation(currentPipeline->programId, name.c_str());
                        if (loc == -1) continue;
                        
                        if (value.type() == typeid(float)) {
                            glUniform1f(loc, std::any_cast<float>(value));
                        } else if (value.type() == typeid(std::array<float, 2>)) {
                            auto arr = std::any_cast<std::array<float, 2>>(value);
                            glUniform2fv(loc, 1, arr.data());
                        } else if (value.type() == typeid(std::array<float, 3>)) {
                            auto arr = std::any_cast<std::array<float, 3>>(value);
                            glUniform3fv(loc, 1, arr.data());
                        } else if (value.type() == typeid(std::array<float, 4>)) {
                            auto arr = std::any_cast<std::array<float, 4>>(value);
                            glUniform4fv(loc, 1, arr.data());
                        } else if (value.type() == typeid(std::array<float, 16>)) {
                            auto arr = std::any_cast<std::array<float, 16>>(value);
                            glUniformMatrix4fv(loc, 1, GL_FALSE, arr.data());
                        } else if (value.type() == typeid(int)) {
                            glUniform1i(loc, std::any_cast<int>(value));
                        } else if (value.type() == typeid(bool)) {
                            glUniform1i(loc, std::any_cast<bool>(value) ? 1 : 0);
                        }
                    }
                }
                break;
                
            case WGL_State_CmdBuffer::Command::Type::Draw:
                if (currentPipeline) {
                    glDrawArrays(gl_topology_webgl(currentPipeline->topology),
                               command.data.draw.fv, command.data.draw.vc);
                }
                break;
                
            case WGL_State_CmdBuffer::Command::Type::DrawIndexed:
                if (currentPipeline) {
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, command.data.drawIdx.indexBuffer);
                    glDrawElements(gl_topology_webgl(currentPipeline->topology),
                                 command.data.drawIdx.ic, GL_UNSIGNED_INT,
                                 reinterpret_cast<void*>(command.data.drawIdx.fi * sizeof(uint32_t)));
                }
                break;
        }
    }
    
    // Reset state
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    check_gl_error("webgl_onCmdExecute");
    
    return AR_result::SUCCESS;
}

static void webgl_onCmdClear(AR_command_buffer* cmd, float r, float g, float b, float a) {
    if (!cmd) return;
    
    auto* cmdState = reinterpret_cast<WGL_State_CmdBuffer*>(cmd);
    if (!cmdState->recording) return;
    
    cmdState->commands.push_back({
        WGL_State_CmdBuffer::Command::Type::Clear,
        {{r, g, b, a}}
    });
}

static void webgl_onCmdSetViewport(AR_command_buffer* cmd, float x, float y, float w, float h) {
    if (!cmd) return;
    
    auto* cmdState = reinterpret_cast<WGL_State_CmdBuffer*>(cmd);
    if (!cmdState->recording) return;
    
    cmdState->commands.push_back({
        WGL_State_CmdBuffer::Command::Type::Viewport,
        {{x, y, w, h}}
    });
}

static void webgl_onCmdBindPipeline(AR_command_buffer* cmd, AR_pipeline* pipeline) {
    if (!cmd || !pipeline) return;
    
    auto* cmdState = reinterpret_cast<WGL_State_CmdBuffer*>(cmd);
    if (!cmdState->recording) return;
    
    cmdState->commands.push_back({
        WGL_State_CmdBuffer::Command::Type::BindPipeline,
        {reinterpret_cast<WGL_State_Pipeline*>(pipeline)}
    });
}

static void webgl_onCmdBindMaterial(AR_command_buffer* cmd, AR_material* material) {
    if (!cmd || !material) return;
    
    auto* cmdState = reinterpret_cast<WGL_State_CmdBuffer*>(cmd);
    if (!cmdState->recording) return;
    
    cmdState->commands.push_back({
        WGL_State_CmdBuffer::Command::Type::BindMaterial,
        {reinterpret_cast<WGL_State_Material*>(material)}
    });
}

static void webgl_onCmdDraw(AR_command_buffer* cmd, uint32_t vc, uint32_t ic, uint32_t fv, uint32_t fi) {
    if (!cmd) return;
    
    auto* cmdState = reinterpret_cast<WGL_State_CmdBuffer*>(cmd);
    if (!cmdState->recording) return;
    
    cmdState->commands.push_back({
        WGL_State_CmdBuffer::Command::Type::Draw,
        {{vc, ic, fv, fi}}
    });
}

static void webgl_onCmdDrawIndexed(AR_command_buffer* cmd, AR_buffer* indexBuffer, 
    uint32_t ic, uint32_t inst, uint32_t fi, int32_t vo, uint32_t fii) {
    
    if (!cmd || !indexBuffer) return;
    
    auto* cmdState = reinterpret_cast<WGL_State_CmdBuffer*>(cmd);
    auto* idxState = reinterpret_cast<WGL_State_Buffer*>(indexBuffer);
    
    if (!cmdState->recording) return;
    
    cmdState->commands.push_back({
        WGL_State_CmdBuffer::Command::Type::DrawIndexed,
        {{idxState->bufferId, ic, inst, fi, vo, fii}}
    });
}

// ============================================================================
// Backend registration
// ============================================================================
static AS_bool32 webgl_is_available() {
    // Check if running in Emscripten with WebGL support
    return emscripten_webgl_get_current_context() != 0;
}

static AR_backend_callbacks g_webgl_callbacks = {
    webgl_onContextInit,
    webgl_onContextUninit,
    webgl_onContextEnumerateDevices,
    webgl_onContextGetDeviceInfo,
    webgl_onDeviceInit,
    webgl_onDeviceUninit,
    webgl_onDeviceStart,
    webgl_onDeviceStop,
    nullptr,  // onDeviceRead
    nullptr,  // onDeviceWrite
    nullptr,  // onDeviceDataLoop
    nullptr,  // onDeviceDataLoopWakeup
    webgl_onSurfaceInit,
    webgl_onSurfaceUninit,
    webgl_onSurfaceResize,
    webgl_onSurfacePresent,
    webgl_onBufferCreate,
    webgl_onBufferDestroy,
    webgl_onTextureCreate,
    webgl_onTextureDestroy,
    webgl_onShaderCreate,
    webgl_onShaderDestroy,
    webgl_onPipelineCreate,
    webgl_onPipelineDestroy,
    webgl_onCmdBufferCreate,
    webgl_onCmdBufferDestroy,
    webgl_onCmdBegin,
    webgl_onCmdEnd,
    webgl_onCmdExecute,
    webgl_onCmdClear,
    webgl_onCmdSetViewport,
    webgl_onCmdBindPipeline,
    webgl_onCmdBindMaterial,
    webgl_onCmdDraw,
    webgl_onCmdDrawIndexed
};

static AR_backend_info g_webgl_info = {
    AR_backend::WEBGPU,  // Placeholder - should add WEBGL enum value
    "WebGL2",
    webgl_is_available,
    &g_webgl_callbacks
};

AR_result AR_register_webgl_backend() {
    return AR_register_backend(&g_webgl_info);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
static void auto_register_webgl() {
    AR_register_webgl_backend();
}
#elif defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void __cdecl auto_register_webgl_msvc() {
    AR_register_webgl_backend();
}
__declspec(allocate(".CRT$XCU")) void (*__auto_register_webgl)(void) = auto_register_webgl_msvc;
#endif

} // namespace arxrender