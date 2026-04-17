// src/backends/backend_metal.cpp
#include "../include/arxrender_backend.hpp"

#if defined(__APPLE__)
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <CoreGraphics/CoreGraphics.h>
#endif

#include <vector>
#include <mutex>
#include <unordered_map>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <any>
#include <array>

namespace arxrender {

#if defined(__APPLE__)

// ============================================================================
// Metal-specific internal structures
// ============================================================================
struct MTL_CommonState {
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> commandQueue = nil;
    std::mutex state_mutex;
};

struct MTL_DeviceState {
    MTL_CommonState* common = nullptr;
    bool initialized = false;
};

struct MTL_SurfaceState {
    CAMetalLayer* metalLayer = nullptr;
    uint32_t width = 800, height = 600;
    MTLPixelFormat pixelFormat = MTLPixelFormatBGRA8Unorm;
    std::vector<id<MTLTexture>> drawableTextures;
    uint32_t currentDrawableIndex = 0;
    bool vsync = true;
};

struct MTL_BufferState {
    id<MTLBuffer> buffer = nil;
    size_t size = 0;
    MTLResourceOptions options = MTLResourceStorageModeShared;
};

struct MTL_TextureState {
    id<MTLTexture> texture = nil;
    uint32_t width = 1, height = 1;
    AR_format format = AR_format::UNDEFINED;
    MTLPixelFormat pixelFormat = MTLPixelFormatRGBA8Unorm;
    bool isRenderTarget = false;
};

struct MTL_ShaderState {
    id<MTLLibrary> library = nil;
    id<MTLFunction> function = nil;
    AR_shader_stage stage = AR_shader_stage::VERTEX;
};

struct MTL_PipelineState {
    id<MTLRenderPipelineState> pipeline = nil;
    AR_primitive_topology topology = AR_primitive_topology::TRIANGLE_LIST;
    AR_blend_state blend;
    AR_depth_stencil_state depth_stencil;
    AR_rasterizer_state rasterizer;
};

struct MTL_MaterialState {
    MTL_PipelineState* pipeline = nullptr;
    std::unordered_map<std::string, MTL_TextureState*> textures;
    std::unordered_map<std::string, std::any> uniforms;
    // Metal uses argument buffers / argument encoders for binding
};

enum class MTL_CmdType { Clear, Viewport, BindPipeline, BindMaterial, Draw, DrawIndexed };
struct MTL_Command {
    MTL_CmdType type;
    union Data {
        struct { float r, g, b, a; } clear;
        struct { float x, y, w, h; } viewport;
        MTL_PipelineState* pipeline;
        MTL_MaterialState* material;
        struct { uint32_t vc, ic, fv, fi; } draw;
        struct { id<MTLBuffer> idxBuf; uint32_t ic, inst, fi; int32_t vo; uint32_t fii; } drawIdx;
        Data() {}
    } data;
};

struct MTL_CmdState {
    id<MTLCommandBuffer> commandBuffer = nil;
    id<MTLRenderCommandEncoder> renderEncoder = nil;
    MTL_CommonState* common = nullptr;
    bool recording = false;
    std::vector<MTL_Command> deferredCommands;
};

// ============================================================================
// Helper functions (format conversions, etc.)
// ============================================================================
static MTLPixelFormat mtl_format(AR_format fmt) {
    switch (fmt) {
        case AR_format::R8_UNORM: return MTLPixelFormatR8Unorm;
        case AR_format::RG8_UNORM: return MTLPixelFormatRG8Unorm;
        case AR_format::RGB8_UNORM: return MTLPixelFormatRGB8Unorm;
        case AR_format::RGBA8_UNORM: return MTLPixelFormatRGBA8Unorm;
        case AR_format::R16_UNORM: return MTLPixelFormatR16Unorm;
        case AR_format::RG16_UNORM: return MTLPixelFormatRG16Unorm;
        case AR_format::RGBA16_UNORM: return MTLPixelFormatRGBA16Unorm;
        case AR_format::R32_FLOAT: return MTLPixelFormatR32Float;
        case AR_format::RG32_FLOAT: return MTLPixelFormatRG32Float;
        case AR_format::RGB32_FLOAT: return MTLPixelFormatRGB32Float;
        case AR_format::RGBA32_FLOAT: return MTLPixelFormatRGBA32Float;
        case AR_format::R32_UINT: return MTLPixelFormatR32Uint;
        case AR_format::RG32_UINT: return MTLPixelFormatRG32Uint;
        case AR_format::RGBA32_UINT: return MTLPixelFormatRGBA32Uint;
        case AR_format::DEPTH32: return MTLPixelFormatDepth32Float;
        case AR_format::DEPTH24_STENCIL8: return MTLPixelFormatDepth24Unorm_Stencil8;
        default: return MTLPixelFormatRGBA8Unorm;
    }
}

static MTLPrimitiveType mtl_topology(AR_primitive_topology t) {
    switch (t) {
        case AR_primitive_topology::POINT_LIST: return MTLPrimitiveTypePoint;
        case AR_primitive_topology::LINE_LIST: return MTLPrimitiveTypeLine;
        case AR_primitive_topology::LINE_STRIP: return MTLPrimitiveTypeLineStrip;
        case AR_primitive_topology::TRIANGLE_LIST: return MTLPrimitiveTypeTriangle;
        case AR_primitive_topology::TRIANGLE_STRIP: return MTLPrimitiveTypeTriangleStrip;
        default: return MTLPrimitiveTypeTriangle;
    }
}

static MTLBlendFactor mtl_blend_factor(AR_blend_factor f) {
    switch (f) {
        case AR_blend_factor::ZERO: return MTLBlendFactorZero;
        case AR_blend_factor::ONE: return MTLBlendFactorOne;
        case AR_blend_factor::SRC_COLOR: return MTLBlendFactorSourceColor;
        case AR_blend_factor::ONE_MINUS_SRC_COLOR: return MTLBlendFactorOneMinusSourceColor;
        case AR_blend_factor::DST_COLOR: return MTLBlendFactorDestinationColor;
        case AR_blend_factor::ONE_MINUS_DST_COLOR: return MTLBlendFactorOneMinusDestinationColor;
        case AR_blend_factor::SRC_ALPHA: return MTLBlendFactorSourceAlpha;
        case AR_blend_factor::ONE_MINUS_SRC_ALPHA: return MTLBlendFactorOneMinusSourceAlpha;
        case AR_blend_factor::DST_ALPHA: return MTLBlendFactorDestinationAlpha;
        case AR_blend_factor::ONE_MINUS_DST_ALPHA: return MTLBlendFactorOneMinusDestinationAlpha;
        default: return MTLBlendFactorOne;
    }
}

static MTLBlendOperation mtl_blend_op(AR_blend_op op) {
    switch (op) {
        case AR_blend_op::ADD: return MTLBlendOperationAdd;
        case AR_blend_op::SUBTRACT: return MTLBlendOperationSubtract;
        case AR_blend_op::REVERSE_SUBTRACT: return MTLBlendOperationReverseSubtract;
        case AR_blend_op::MIN: return MTLBlendOperationMin;
        case AR_blend_op::MAX: return MTLBlendOperationMax;
        default: return MTLBlendOperationAdd;
    }
}

static MTLCompareFunction mtl_compare_op(AR_compare_op op) {
    switch (op) {
        case AR_compare_op::NEVER: return MTLCompareFunctionNever;
        case AR_compare_op::LESS: return MTLCompareFunctionLess;
        case AR_compare_op::EQUAL: return MTLCompareFunctionEqual;
        case AR_compare_op::LESS_EQUAL: return MTLCompareFunctionLessEqual;
        case AR_compare_op::GREATER: return MTLCompareFunctionGreater;
        case AR_compare_op::NOT_EQUAL: return MTLCompareFunctionNotEqual;
        case AR_compare_op::GREATER_EQUAL: return MTLCompareFunctionGreaterEqual;
        case AR_compare_op::ALWAYS: return MTLCompareFunctionAlways;
        default: return MTLCompareFunctionLess;
    }
}

static MTLCullMode mtl_cull_mode(AR_cull_mode mode) {
    switch (mode) {
        case AR_cull_mode::NONE: return MTLCullModeNone;
        case AR_cull_mode::FRONT: return MTLCullModeFront;
        case AR_cull_mode::BACK: return MTLCullModeBack;
        default: return MTLCullModeBack;
    }
}

static MTLWinding mtl_front_face(AR_front_face face) {
    return (face == AR_front_face::CW) ? MTLWindingClockwise : MTLWindingCounterClockwise;
}

static MTLPolygonMode mtl_polygon_mode(AR_polygon_mode mode) {
    // Metal only supports fill mode
    (void)mode;
    return MTLPolygonModeFill;
}

// ============================================================================
// Backend callbacks implementation
// ============================================================================

// --- Context callbacks ---
static AR_result mtl_onContextInit(AR_context*, const AR_context_config*) {
    // Metal context initialization is minimal — device is created per-device
    return AR_result::SUCCESS;
}

static AR_result mtl_onContextUninit(AR_context*) {
    return AR_result::SUCCESS;
}

static AR_result mtl_onContextEnumerateDevices(AR_context*, AR_device_type, 
    AR_enumerate_devices_callback, void*) {
    return AR_result::NOT_IMPLEMENTED;
}

static AR_result mtl_onContextGetDeviceInfo(AR_context*, AR_device_type, 
    const void*, AR_device_info*) {
    return AR_result::NOT_IMPLEMENTED;
}

// --- Device callbacks ---
static AR_result mtl_onDeviceInit(AR_context*, AR_device_type, const void*, 
    const AR_device_config*, AR_device* device) {
    
    if (!device) return AR_result::INVALID_ARGS;
    
    auto* devState = new(std::nothrow) MTL_DeviceState{};
    if (!devState) return AR_result::OUT_OF_MEMORY;
    
    auto* common = new(std::nothrow) MTL_CommonState{};
    if (!common) {
        delete devState;
        return AR_result::OUT_OF_MEMORY;
    }
    
    // Create Metal device
    id<MTLDevice> metalDevice = MTLCreateSystemDefaultDevice();
    if (!metalDevice) {
        delete common;
        delete devState;
        return AR_result::NO_BACKEND;
    }
    
    common->device = metalDevice;
    common->commandQueue = [metalDevice newCommandQueue];
    
    devState->common = common;
    devState->initialized = true;
    
    device->p_impl->backend_data = static_cast<void*>(devState);
    return AR_result::SUCCESS;
}

static AR_result mtl_onDeviceUninit(AR_device* device) {
    if (!device || !device->p_impl) return AR_result::INVALID_ARGS;
    
    auto* devState = static_cast<MTL_DeviceState*>(device->p_impl->backend_data);
    if (!devState) return AR_result::SUCCESS;
    
    if (devState->common) {
        if (devState->common->commandQueue) {
            [devState->common->commandQueue release];
        }
        if (devState->common->device) {
            [devState->common->device release];
        }
        delete devState->common;
    }
    
    delete devState;
    device->p_impl->backend_data = nullptr;
    
    return AR_result::SUCCESS;
}

static AR_result mtl_onDeviceStart(AR_device*) { return AR_result::SUCCESS; }
static AR_result mtl_onDeviceStop(AR_device*) { return AR_result::SUCCESS; }

// --- Surface callbacks ---
static AR_result mtl_onSurfaceInit(AR_context*, const AR_surface_config* config, AR_surface* surface) {
    if (!config || !surface) return AR_result::INVALID_ARGS;
    
    auto* surfState = new(std::nothrow) MTL_SurfaceState{};
    if (!surfState) return AR_result::OUT_OF_MEMORY;
    
    // Create CAMetalLayer
    surfState->metalLayer = [CAMetalLayer layer];
    if (!surfState->metalLayer) {
        delete surfState;
        return AR_result::ERROR_GENERIC;
    }
    
    surfState->metalLayer.device = [MTLCreateSystemDefaultDevice() autorelease];
    surfState->metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    surfState->metalLayer.framebufferOnly = YES;
    surfState->metalLayer.drawableSize = CGSizeMake(config->width, config->height);
    surfState->metalLayer.presentsWithTransaction = NO;
    
    surfState->width = config->width;
    surfState->height = config->height;
    surfState->vsync = config->vsync;
    surfState->pixelFormat = MTLPixelFormatBGRA8Unorm;
    
    surface->p_impl->backend_data = static_cast<void*>(surfState);
    return AR_result::SUCCESS;
}

static AR_result mtl_onSurfaceUninit(AR_surface* surface) {
    if (!surface || !surface->p_impl) return AR_result::INVALID_ARGS;
    
    auto* surfState = static_cast<MTL_SurfaceState*>(surface->p_impl->backend_data);
    if (surfState) {
        if (surfState->metalLayer) {
            [surfState->metalLayer release];
        }
        delete surfState;
    }
    surface->p_impl->backend_data = nullptr;
    
    return AR_result::SUCCESS;
}

static AR_result mtl_onSurfaceResize(AR_surface* surface, uint32_t width, uint32_t height) {
    if (!surface || !surface->p_impl) return AR_result::INVALID_ARGS;
    
    auto* surfState = static_cast<MTL_SurfaceState*>(surface->p_impl->backend_data);
    if (!surfState) return AR_result::INVALID_OPERATION;
    
    surfState->width = width;
    surfState->height = height;
    
    if (surfState->metalLayer) {
        surfState->metalLayer.drawableSize = CGSizeMake(width, height);
    }
    
    return AR_result::SUCCESS;
}

static AR_result mtl_onSurfacePresent(AR_surface* surface, AR_command_buffer* cmd) {
    if (!surface || !surface->p_impl || !cmd) return AR_result::INVALID_ARGS;
    
    auto* surfState = static_cast<MTL_SurfaceState*>(surface->p_impl->backend_data);
    auto* cmdState = static_cast<MTL_CmdState*>(cmd->p_impl->backend_data);
    
    if (!surfState || !cmdState) return AR_result::INVALID_OPERATION;
    
    // Commit and present the command buffer
    if (cmdState->commandBuffer) {
        [cmdState->commandBuffer commit];
        
        if (surfState->vsync) {
            [cmdState->commandBuffer waitUntilCompleted];
        }
    }
    
    return AR_result::SUCCESS;
}

// --- Resource callbacks ---
static AR_result mtl_onBufferCreate(AR_device* device, const AR_buffer_desc* desc, AR_buffer** out) {
    if (!device || !desc || !out) return AR_result::INVALID_ARGS;
    
    auto* devState = static_cast<MTL_DeviceState*>(device->p_impl->backend_data);
    if (!devState || !devState->common) return AR_result::INVALID_OPERATION;
    
    auto* bufState = new(std::nothrow) MTL_BufferState{};
    if (!bufState) return AR_result::OUT_OF_MEMORY;
    
    bufState->size = desc->size;
    
    // Determine storage mode
    if (desc->host_visible) {
        bufState->options = MTLResourceStorageModeShared;
    } else {
        bufState->options = MTLResourceStorageModePrivate;
    }
    
    // Create buffer
    bufState->buffer = [devState->common->device 
        newBufferWithLength:desc->size 
        options:bufState->options];
    
    if (!bufState->buffer) {
        delete bufState;
        return AR_result::ERROR_GENERIC;
    }
    
    // Upload initial data if provided and host-visible
    if (desc->initial_data && desc->host_visible) {
        void* contents = [bufState->buffer contents];
        memcpy(contents, desc->initial_data, desc->size);
    }
    
    *out = reinterpret_cast<AR_buffer*>(bufState);
    return AR_result::SUCCESS;
}

static void mtl_onBufferDestroy(AR_buffer* buffer) {
    if (!buffer) return;
    
    auto* bufState = reinterpret_cast<MTL_BufferState*>(buffer);
    if (bufState->buffer) {
        [bufState->buffer release];
    }
    delete bufState;
}

static AR_result mtl_onTextureCreate(AR_device* device, const AR_texture_desc* desc, AR_texture** out) {
    if (!device || !desc || !out) return AR_result::INVALID_ARGS;
    
    auto* devState = static_cast<MTL_DeviceState*>(device->p_impl->backend_data);
    if (!devState || !devState->common) return AR_result::INVALID_OPERATION;
    
    auto* texState = new(std::nothrow) MTL_TextureState{};
    if (!texState) return AR_result::OUT_OF_MEMORY;
    
    texState->width = desc->width;
    texState->height = desc->height;
    texState->format = desc->format;
    texState->pixelFormat = mtl_format(desc->format);
    texState->isRenderTarget = (desc->usage & (AR_usage::COLOR_ATTACHMENT | AR_usage::DEPTH_STENCIL_ATTACHMENT)) != AR_usage(0);
    
    // Create texture descriptor
    MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:texState->pixelFormat
                                                                                       width:desc->width
                                                                                      height:desc->height
                                                                                   mipmapped:(desc->mip_levels > 1)];
    texDesc.usage = MTLTextureUsageShaderRead;
    if (texState->isRenderTarget) {
        texDesc.usage |= MTLTextureUsageRenderTarget;
    }
    if (desc->usage & AR_usage::STORAGE_TEXTURE) {
        texDesc.usage |= MTLTextureUsageShaderWrite;
    }
    
    // Create texture
    texState->texture = [devState->common->device newTextureWithDescriptor:texDesc];
    if (!texState->texture) {
        delete texState;
        return AR_result::ERROR_GENERIC;
    }
    
    // Upload initial data if provided
    if (desc->initial_data) {
        size_t bytesPerRow = desc->width * 4; // Assume RGBA
        [texState->texture replaceRegion:MTLRegionMake2D(0, 0, desc->width, desc->height)
                             mipmapLevel:0
                           withBytes:desc->initial_data
                         bytesPerRow:bytesPerRow];
    }
    
    *out = reinterpret_cast<AR_texture*>(texState);
    return AR_result::SUCCESS;
}

static void mtl_onTextureDestroy(AR_texture* texture) {
    if (!texture) return;
    
    auto* texState = reinterpret_cast<MTL_TextureState*>(texture);
    if (texState->texture) {
        [texState->texture release];
    }
    delete texState;
}

static AR_result mtl_onShaderCreate(AR_device*, AR_shader_stage stage, 
    const void* code, size_t size, const char* entryPoint, AR_shader** out) {
    
    if (!code || !size || !entryPoint || !out) return AR_result::INVALID_ARGS;
    
    auto* shState = new(std::nothrow) MTL_ShaderState{};
    if (!shState) return AR_result::OUT_OF_MEMORY;
    
    shState->stage = stage;
    
    // Create library from bytecode (Metal uses precompiled .metallib or source)
    // For simplicity, assume source code is provided as Metal shading language
    NSError* error = nil;
    id<MTLLibrary> library = [[MTLCreateSystemDefaultDevice() newLibraryWithSource:[NSString stringWithUTF8String:static_cast<const char*>(code)] 
                                                                           options:nil 
                                                                             error:&error] autorelease];
    
    if (!library) {
        std::cerr << "[ArxRender/Metal] Shader compile error: " << [[error localizedDescription] UTF8String] << std::endl;
        delete shState;
        return AR_result::PIPELINE_COMPILE_FAILED;
    }
    
    shState->library = library;
    shState->function = [library newFunctionWithName:[NSString stringWithUTF8String:entryPoint]];
    
    if (!shState->function) {
        [library release];
        delete shState;
        return AR_result::PIPELINE_COMPILE_FAILED;
    }
    
    *out = reinterpret_cast<AR_shader*>(shState);
    return AR_result::SUCCESS;
}

static void mtl_onShaderDestroy(AR_shader* shader) {
    if (!shader) return;
    
    auto* shState = reinterpret_cast<MTL_ShaderState*>(shader);
    if (shState->function) {
        [shState->function release];
    }
    if (shState->library) {
        [shState->library release];
    }
    delete shState;
}

static AR_result mtl_onPipelineCreate(AR_device* device, const AR_pipeline_desc* desc, AR_pipeline** out) {
    if (!device || !desc || !out) return AR_result::INVALID_ARGS;
    
    auto* devState = static_cast<MTL_DeviceState*>(device->p_impl->backend_data);
    auto* surfState = static_cast<MTL_SurfaceState*>(device->p_impl->context->p_impl->backend_data);
    if (!devState || !devState->common || !surfState) return AR_result::INVALID_OPERATION;
    
    auto* pipeState = new(std::nothrow) MTL_PipelineState{};
    if (!pipeState) return AR_result::OUT_OF_MEMORY;
    
    pipeState->topology = desc->topology;
    pipeState->blend = desc->blend;
    pipeState->depth_stencil = desc->depth_stencil;
    pipeState->rasterizer = desc->rasterizer;
    
    // Create render pipeline descriptor
    MTLRenderPipelineDescriptor* pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDesc.label = @"ArxRenderPipeline";
    
    // Vertex function
    if (desc->vertex_shader) {
        auto* vs = reinterpret_cast<MTL_ShaderState*>(desc->vertex_shader);
        pipelineDesc.vertexFunction = vs->function;
    }
    
    // Fragment function
    if (desc->fragment_shader) {
        auto* fs = reinterpret_cast<MTL_ShaderState*>(desc->fragment_shader);
        pipelineDesc.fragmentFunction = fs->function;
    }
    
    // Color attachment
    pipelineDesc.colorAttachments[0].pixelFormat = surfState->pixelFormat;
    pipelineDesc.colorAttachments[0].blendingEnabled = desc->blend.enabled ? YES : NO;
    pipelineDesc.colorAttachments[0].sourceRGBBlendFactor = mtl_blend_factor(desc->blend.src_color);
    pipelineDesc.colorAttachments[0].destinationRGBBlendFactor = mtl_blend_factor(desc->blend.dst_color);
    pipelineDesc.colorAttachments[0].rgbBlendOperation = mtl_blend_op(desc->blend.color_op);
    pipelineDesc.colorAttachments[0].sourceAlphaBlendFactor = mtl_blend_factor(desc->blend.src_alpha);
    pipelineDesc.colorAttachments[0].destinationAlphaBlendFactor = mtl_blend_factor(desc->blend.dst_alpha);
    pipelineDesc.colorAttachments[0].alphaBlendOperation = mtl_blend_op(desc->blend.alpha_op);
    pipelineDesc.colorAttachments[0].writeMask = MTLColorWriteMaskAll;
    
    // Depth stencil
    pipelineDesc.depthAttachmentPixelFormat = desc->depth_stencil.depth_test ? MTLPixelFormatDepth32Float : MTLPixelFormatInvalid;
    pipelineDesc.stencilAttachmentPixelFormat = desc->depth_stencil.stencil_test ? MTLPixelFormatStencil8 : MTLPixelFormatInvalid;
    
    // Rasterization
    pipelineDesc.rasterSampleCount = 1;
    pipelineDesc.rasterizationEnabled = YES;
    
    // Create pipeline state
    NSError* error = nil;
    id<MTLRenderPipelineState> pipeline = [devState->common->device 
        newRenderPipelineStateWithDescriptor:pipelineDesc 
                                       error:&error];
    
    [pipelineDesc release];
    
    if (!pipeline) {
        std::cerr << "[ArxRender/Metal] Pipeline creation error: " << [[error localizedDescription] UTF8String] << std::endl;
        delete pipeState;
        return AR_result::PIPELINE_COMPILE_FAILED;
    }
    
    pipeState->pipeline = pipeline;
    
    *out = reinterpret_cast<AR_pipeline*>(pipeState);
    return AR_result::SUCCESS;
}

static void mtl_onPipelineDestroy(AR_pipeline* pipeline) {
    if (!pipeline) return;
    
    auto* pipeState = reinterpret_cast<MTL_PipelineState*>(pipeline);
    if (pipeState->pipeline) {
        [pipeState->pipeline release];
    }
    delete pipeState;
}

// --- Command buffer callbacks ---
static AR_result mtl_onCmdBufferCreate(AR_device* device, AR_command_buffer** out) {
    if (!device || !out) return AR_result::INVALID_ARGS;
    
    auto* devState = static_cast<MTL_DeviceState*>(device->p_impl->backend_data);
    if (!devState || !devState->common) return AR_result::INVALID_OPERATION;
    
    auto* cmdState = new(std::nothrow) MTL_CmdState{};
    if (!cmdState) return AR_result::OUT_OF_MEMORY;
    
    cmdState->common = devState->common;
    
    *out = reinterpret_cast<AR_command_buffer*>(cmdState);
    return AR_result::SUCCESS;
}

static void mtl_onCmdBufferDestroy(AR_command_buffer* cmd) {
    if (!cmd) return;
    
    auto* cmdState = reinterpret_cast<MTL_CmdState*>(cmd);
    if (cmdState->renderEncoder) {
        [cmdState->renderEncoder endEncoding];
        [cmdState->renderEncoder release];
    }
    if (cmdState->commandBuffer) {
        [cmdState->commandBuffer release];
    }
    delete cmdState;
}

static AR_result mtl_onCmdBegin(AR_command_buffer* cmd) {
    if (!cmd) return AR_result::INVALID_ARGS;
    
    auto* cmdState = reinterpret_cast<MTL_CmdState*>(cmd);
    if (cmdState->recording) return AR_result::INVALID_OPERATION;
    
    // Create new command buffer
    cmdState->commandBuffer = [cmdState->common->commandQueue commandBuffer];
    if (!cmdState->commandBuffer) {
        return AR_result::ERROR_GENERIC;
    }
    
    cmdState->recording = true;
    return AR_result::SUCCESS;
}

static AR_result mtl_onCmdEnd(AR_command_buffer* cmd) {
    if (!cmd) return AR_result::INVALID_ARGS;
    
    auto* cmdState = reinterpret_cast<MTL_CmdState*>(cmd);
    if (!cmdState->recording) return AR_result::INVALID_OPERATION;
    
    // End render encoder if active
    if (cmdState->renderEncoder) {
        [cmdState->renderEncoder endEncoding];
        [cmdState->renderEncoder release];
        cmdState->renderEncoder = nil;
    }
    
    cmdState->recording = false;
    return AR_result::SUCCESS;
}

static AR_result mtl_onCmdExecute(AR_command_buffer* cmd) {
    if (!cmd) return AR_result::INVALID_ARGS;
    
    auto* cmdState = reinterpret_cast<MTL_CmdState*>(cmd);
    
    // Commit the command buffer
    if (cmdState->commandBuffer) {
        [cmdState->commandBuffer commit];
    }
    
    return AR_result::SUCCESS;
}

static void mtl_onCmdClear(AR_command_buffer* cmd, float r, float g, float b, float a) {
    if (!cmd) return;
    
    auto* cmdState = reinterpret_cast<MTL_CmdState*>(cmd);
    if (!cmdState->recording) return;
    
    // Store for deferred execution
    cmdState->deferredCommands.push_back({
        MTL_CmdType::Clear,
        {{r, g, b, a}}
    });
}

static void mtl_onCmdSetViewport(AR_command_buffer* cmd, float x, float y, float w, float h) {
    if (!cmd) return;
    
    auto* cmdState = reinterpret_cast<MTL_CmdState*>(cmd);
    if (!cmdState->recording) return;
    
    cmdState->deferredCommands.push_back({
        MTL_CmdType::Viewport,
        {{x, y, w, h}}
    });
}

static void mtl_onCmdBindPipeline(AR_command_buffer* cmd, AR_pipeline* pipeline) {
    if (!cmd || !pipeline) return;
    
    auto* cmdState = reinterpret_cast<MTL_CmdState*>(cmd);
    if (!cmdState->recording) return;
    
    cmdState->deferredCommands.push_back({
        MTL_CmdType::BindPipeline,
        {reinterpret_cast<MTL_PipelineState*>(pipeline)}
    });
}

static void mtl_onCmdBindMaterial(AR_command_buffer* cmd, AR_material* material) {
    if (!cmd || !material) return;
    
    auto* cmdState = reinterpret_cast<MTL_CmdState*>(cmd);
    if (!cmdState->recording) return;
    
    cmdState->deferredCommands.push_back({
        MTL_CmdType::BindMaterial,
        {reinterpret_cast<MTL_MaterialState*>(material)}
    });
}

static void mtl_onCmdDraw(AR_command_buffer* cmd, uint32_t vc, uint32_t ic, uint32_t fv, uint32_t fi) {
    if (!cmd) return;
    
    auto* cmdState = reinterpret_cast<MTL_CmdState*>(cmd);
    if (!cmdState->recording) return;
    
    cmdState->deferredCommands.push_back({
        MTL_CmdType::Draw,
        {{vc, ic, fv, fi}}
    });
}

static void mtl_onCmdDrawIndexed(AR_command_buffer* cmd, AR_buffer* indexBuffer, 
    uint32_t ic, uint32_t inst, uint32_t fi, int32_t vo, uint32_t fii) {
    
    if (!cmd || !indexBuffer) return;
    
    auto* cmdState = reinterpret_cast<MTL_CmdState*>(cmd);
    auto* idxState = reinterpret_cast<MTL_BufferState*>(indexBuffer);
    
    if (!cmdState->recording) return;
    
    cmdState->deferredCommands.push_back({
        MTL_CmdType::DrawIndexed,
        {{idxState->buffer, ic, inst, fi, vo, fii}}
    });
}

#else // !__APPLE__

// Stub implementations for non-Apple platforms
static AR_result mtl_onContextInit(AR_context*, const AR_context_config*) { return AR_result::NO_BACKEND; }
static AR_result mtl_onContextUninit(AR_context*) { return AR_result::SUCCESS; }
static AR_result mtl_onContextEnumerateDevices(AR_context*, AR_device_type, AR_enumerate_devices_callback, void*) { return AR_result::NOT_IMPLEMENTED; }
static AR_result mtl_onContextGetDeviceInfo(AR_context*, AR_device_type, const void*, AR_device_info*) { return AR_result::NOT_IMPLEMENTED; }
static AR_result mtl_onDeviceInit(AR_context*, AR_device_type, const void*, const AR_device_config*, AR_device*) { return AR_result::NO_BACKEND; }
static AR_result mtl_onDeviceUninit(AR_device*) { return AR_result::SUCCESS; }
static AR_result mtl_onDeviceStart(AR_device*) { return AR_result::SUCCESS; }
static AR_result mtl_onDeviceStop(AR_device*) { return AR_result::SUCCESS; }
static AR_result mtl_onSurfaceInit(AR_context*, const AR_surface_config*, AR_surface*) { return AR_result::NO_BACKEND; }
static AR_result mtl_onSurfaceUninit(AR_surface*) { return AR_result::SUCCESS; }
static AR_result mtl_onSurfaceResize(AR_surface*, uint32_t, uint32_t) { return AR_result::SUCCESS; }
static AR_result mtl_onSurfacePresent(AR_surface*, AR_command_buffer*) { return AR_result::SUCCESS; }
static AR_result mtl_onBufferCreate(AR_device*, const AR_buffer_desc*, AR_buffer**) { return AR_result::NO_BACKEND; }
static void mtl_onBufferDestroy(AR_buffer*) {}
static AR_result mtl_onTextureCreate(AR_device*, const AR_texture_desc*, AR_texture**) { return AR_result::NO_BACKEND; }
static void mtl_onTextureDestroy(AR_texture*) {}
static AR_result mtl_onShaderCreate(AR_device*, AR_shader_stage, const void*, size_t, const char*, AR_shader**) { return AR_result::NO_BACKEND; }
static void mtl_onShaderDestroy(AR_shader*) {}
static AR_result mtl_onPipelineCreate(AR_device*, const AR_pipeline_desc*, AR_pipeline**) { return AR_result::NO_BACKEND; }
static void mtl_onPipelineDestroy(AR_pipeline*) {}
static AR_result mtl_onCmdBufferCreate(AR_device*, AR_command_buffer**) { return AR_result::NO_BACKEND; }
static void mtl_onCmdBufferDestroy(AR_command_buffer*) {}
static AR_result mtl_onCmdBegin(AR_command_buffer*) { return AR_result::NO_BACKEND; }
static AR_result mtl_onCmdEnd(AR_command_buffer*) { return AR_result::NO_BACKEND; }
static AR_result mtl_onCmdExecute(AR_command_buffer*) { return AR_result::NO_BACKEND; }
static void mtl_onCmdClear(AR_command_buffer*, float, float, float, float) {}
static void mtl_onCmdSetViewport(AR_command_buffer*, float, float, float, float) {}
static void mtl_onCmdBindPipeline(AR_command_buffer*, AR_pipeline*) {}
static void mtl_onCmdBindMaterial(AR_command_buffer*, AR_material*) {}
static void mtl_onCmdDraw(AR_command_buffer*, uint32_t, uint32_t, uint32_t, uint32_t) {}
static void mtl_onCmdDrawIndexed(AR_command_buffer*, AR_buffer*, uint32_t, uint32_t, uint32_t, int32_t, uint32_t) {}

#endif // __APPLE__

// ============================================================================
// Backend registration
// ============================================================================
static AS_bool32 mtl_is_available() {
#if defined(__APPLE__)
    return MTLCreateSystemDefaultDevice() != nil;
#else
    return AS_FALSE;
#endif
}

static AR_backend_callbacks g_mtl_callbacks = {
    mtl_onContextInit,
    mtl_onContextUninit,
    mtl_onContextEnumerateDevices,
    mtl_onContextGetDeviceInfo,
    mtl_onDeviceInit,
    mtl_onDeviceUninit,
    mtl_onDeviceStart,
    mtl_onDeviceStop,
    nullptr,  // onDeviceRead
    nullptr,  // onDeviceWrite
    nullptr,  // onDeviceDataLoop
    nullptr,  // onDeviceDataLoopWakeup
    mtl_onSurfaceInit,
    mtl_onSurfaceUninit,
    mtl_onSurfaceResize,
    mtl_onSurfacePresent,
    mtl_onBufferCreate,
    mtl_onBufferDestroy,
    mtl_onTextureCreate,
    mtl_onTextureDestroy,
    mtl_onShaderCreate,
    mtl_onShaderDestroy,
    mtl_onPipelineCreate,
    mtl_onPipelineDestroy,
    mtl_onCmdBufferCreate,
    mtl_onCmdBufferDestroy,
    mtl_onCmdBegin,
    mtl_onCmdEnd,
    mtl_onCmdExecute,
    mtl_onCmdClear,
    mtl_onCmdSetViewport,
    mtl_onCmdBindPipeline,
    mtl_onCmdBindMaterial,
    mtl_onCmdDraw,
    mtl_onCmdDrawIndexed
};

static AR_backend_info g_mtl_info = {
    AR_backend::METAL,
    "Metal",
    mtl_is_available,
    &g_mtl_callbacks
};

AR_result AR_register_metal_backend() {
    return AR_register_backend(&g_mtl_info);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
static void auto_register_metal() {
    AR_register_metal_backend();
}
#elif defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void __cdecl auto_register_metal_msvc() {
    AR_register_metal_backend();
}
__declspec(allocate(".CRT$XCU")) void (*__auto_register_metal)(void) = auto_register_metal_msvc;
#endif

} // namespace arxrender