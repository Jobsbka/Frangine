#pragma once
#include "arxrender_types.hpp"
#include "arxrender_device.hpp"
#include "arxrender_resource.hpp"
#include <string>
#include <unordered_map>
#include <any>

namespace arxrender {

class AR_shader {
public:
    AR_API AR_shader();
    AR_API ~AR_shader();
    // opaque handle internally
private:
    struct impl;
    std::unique_ptr<impl> p_impl;
};

struct AR_pipeline_desc {
    AR_primitive_topology topology = AR_primitive_topology::TRIANGLE_LIST;
    AR_shader* vertex_shader = nullptr;
    AR_shader* fragment_shader = nullptr;
    AR_blend_state blend = {};
    AR_depth_stencil_state depth_stencil = {};
    AR_rasterizer_state rasterizer = {};
};

class AR_pipeline {
public:
    AR_API AR_pipeline();
    AR_API ~AR_pipeline();
private:
    struct impl;
    std::unique_ptr<impl> p_impl;
};

class AR_material {
public:
    AR_API AR_material(AR_pipeline* pipeline);
    AR_API ~AR_material();
    AR_API void set_texture(const std::string& name, AR_texture* tex);
    AR_API void set_uniform(const std::string& name, const std::any& value);
    AR_API AR_pipeline* pipeline() const;

private:
    struct impl;
    std::unique_ptr<impl> p_impl;
};

} // namespace arxrender