#pragma once
#include <cstdint>
#include <cstddef>

namespace arxrender {

enum class AR_result : int32_t {
    SUCCESS = 0,
    ERROR_GENERIC = -1,
    INVALID_ARGS = -2,
    INVALID_OPERATION = -3,
    NO_BACKEND = -4,
    OUT_OF_MEMORY = -5,
    NOT_IMPLEMENTED = -6,
    DEVICE_LOST = -7,
    FORMAT_NOT_SUPPORTED = -8,
    PIPELINE_COMPILE_FAILED = -9
};

enum class AR_backend : uint32_t {
    NULL_BACKEND = 0,
    OPENGL = 1,
    VULKAN = 2,
    METAL = 3,
    D3D12 = 4,
    WEBGL = 5
};

enum class AR_format : uint32_t {
    UNDEFINED = 0,
    R8_UNORM, RG8_UNORM, RGB8_UNORM, RGBA8_UNORM,
    R16_UNORM, RG16_UNORM, RGBA16_UNORM,
    R32_FLOAT, RG32_FLOAT, RGB32_FLOAT, RGBA32_FLOAT,
    R32_UINT, RG32_UINT, RGBA32_UINT,
    DEPTH32, DEPTH24_STENCIL8,
    BC1_RGB, BC3_RGBA
};

enum class AR_usage : uint32_t {
    TRANSFER_SRC = 1 << 0, TRANSFER_DST = 1 << 1, UNIFORM_BUFFER = 1 << 2,
    STORAGE_BUFFER = 1 << 3, VERTEX_BUFFER = 1 << 4, INDEX_BUFFER = 1 << 5,
    COLOR_ATTACHMENT = 1 << 6, DEPTH_STENCIL_ATTACHMENT = 1 << 7,
    SAMPLED_TEXTURE = 1 << 8, STORAGE_TEXTURE = 1 << 9
};
inline AR_usage operator|(AR_usage a, AR_usage b) { return static_cast<AR_usage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); }

enum class AR_primitive_topology : uint8_t { POINT_LIST, LINE_LIST, LINE_STRIP, TRIANGLE_LIST, TRIANGLE_STRIP, TRIANGLE_FAN };
enum class AR_cull_mode : uint8_t { NONE, FRONT, BACK };
enum class AR_front_face : uint8_t { CW, CCW };
enum class AR_blend_op : uint8_t { ADD, SUBTRACT, REVERSE_SUBTRACT, MIN, MAX };
enum class AR_blend_factor : uint8_t { ZERO, ONE, SRC_COLOR, ONE_MINUS_SRC_COLOR, DST_COLOR, ONE_MINUS_DST_COLOR, SRC_ALPHA, ONE_MINUS_SRC_ALPHA, DST_ALPHA, ONE_MINUS_DST_ALPHA };
enum class AR_compare_op : uint8_t { NEVER, LESS, EQUAL, LESS_EQUAL, GREATER, NOT_EQUAL, GREATER_EQUAL, ALWAYS };
enum class AR_shader_stage : uint8_t { VERTEX, FRAGMENT, COMPUTE, GEOMETRY, TESSELLATION_CTRL, TESSELLATION_EVAL };
enum class AR_polygon_mode : uint8_t { FILL, LINE, POINT };

struct AR_blend_state {
    bool enabled = false;
    AR_blend_op color_op = AR_blend_op::ADD, alpha_op = AR_blend_op::ADD;
    AR_blend_factor src_color = AR_blend_factor::SRC_ALPHA, dst_color = AR_blend_factor::ONE_MINUS_SRC_ALPHA;
    AR_blend_factor src_alpha = AR_blend_factor::ONE, dst_alpha = AR_blend_factor::ZERO;
};

struct AR_depth_stencil_state {
    bool depth_test = false, depth_write = true, stencil_test = false;
    AR_compare_op depth_compare = AR_compare_op::LESS;
};

struct AR_rasterizer_state {
    AR_cull_mode cull_mode = AR_cull_mode::BACK;
    AR_front_face front_face = AR_front_face::CCW;
    AR_polygon_mode polygon_mode = AR_polygon_mode::FILL;
    bool depth_bias = false;
    float depth_bias_constant = 0.0f, depth_bias_clamp = 0.0f, depth_bias_slope = 0.0f;
};

} // namespace arxrender