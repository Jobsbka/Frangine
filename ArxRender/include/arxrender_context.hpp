#pragma once
#include "arxrender_types.hpp"
#include <memory>
#include <mutex>
#include <vector>

namespace arxrender {
struct AR_backend_callbacks;
struct AR_backend_info;
class AR_log;

struct AR_context_config {
    AR_log* log = nullptr;
    void* (*on_alloc)(size_t, void*) = nullptr;
    void (*on_free)(void*, void*) = nullptr;
    void* alloc_user_data = nullptr;
    bool debug_layer = false;
};

class AR_context {
public:
    AR_API AR_context();
    AR_API ~AR_context();
    AR_API AR_result init(const AR_backend* priority_list = nullptr, uint32_t count = 0, const AR_context_config* config = nullptr);
    AR_API void uninit();
    AR_API AR_backend active_backend() const;
    AR_API static const char* backend_name(AR_backend backend);
    AR_API static AR_result backend_from_name(const char* name, AR_backend* out);
    AR_API AR_log* log() const;
    AR_API const AR_backend_callbacks* callbacks() const;

private:
    struct impl;
    std::unique_ptr<impl> p_impl;
};
} // namespace arxrender