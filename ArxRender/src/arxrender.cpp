#include "../include/arxrender.hpp"
#include "../include/arxrender_backend.hpp"
#include <mutex>
#include <vector>

namespace arxrender {
struct GlobalState {
    std::mutex backend_mutex;
    std::vector<AR_backend_info> registered_backends;
};
static GlobalState& get_global_state() {
    static GlobalState state;
    return state;
}

AR_version get_version() {
    return {ARXRENDER_VERSION_MAJOR, ARXRENDER_VERSION_MINOR, ARXRENDER_VERSION_PATCH};
}

AR_result AR_register_backend(const AR_backend_info* info) {
    if (!info) return AR_result::INVALID_ARGS;
    auto& state = get_global_state();
    std::lock_guard<std::mutex> lock(state.backend_mutex);
    for (const auto& existing : state.registered_backends) {
        if (existing.backend == info->backend) return AR_result::INVALID_OPERATION;
    }
    state.registered_backends.push_back(*info);
    return AR_result::SUCCESS;
}
} // namespace arxrender