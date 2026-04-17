#pragma once
#include "arxrender_types.hpp"
#include "arxrender_device.hpp"
#include <memory>

namespace arxrender {
class AR_command_buffer {
public:
    AR_API AR_command_buffer();
    AR_API ~AR_command_buffer();
    AR_API AR_result begin();
    AR_API AR_result end();
    AR_API AR_result execute();
    AR_API void clear_color(float r, float g, float b, float a);
    AR_API void set_viewport(float x, float y, float w, float h);
    AR_API void bind_pipeline(AR_pipeline* pipeline);
    AR_API void bind_material(AR_material* material);
    AR_API void draw(uint32_t vc, uint32_t ic = 1, uint32_t fv = 0, uint32_t fi = 0);
    AR_API void draw_indexed(AR_buffer* idx, uint32_t ic, uint32_t inst = 1, uint32_t fi = 0, int32_t vo = 0, uint32_t fii = 0);
    AR_API AR_device* device() const;
    AR_API void* backend_data() const;
    AR_API void set_backend_data(void* data);

private:
    struct impl;
    std::unique_ptr<impl> p_impl;
};
}