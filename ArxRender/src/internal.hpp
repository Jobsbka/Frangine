#pragma once
#include <mutex>
#include <vector>
namespace arxrender {
    struct AR_backend_info;
    struct GlobalState {
        std::mutex backend_mutex;
        std::vector<AR_backend_info> registered_backends;
    };
    GlobalState& get_global_state();
}