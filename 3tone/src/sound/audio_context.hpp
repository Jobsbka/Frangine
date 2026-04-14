#pragma once
#include "../core/context.hpp"
#include "audio_spec.hpp"
#include <memory>

namespace arxglue::sound {

class AudioDevice;

struct AudioContextData {
    std::shared_ptr<AudioDevice> device;
    AudioSpec spec;
    bool isRealtime = true;
};

inline AudioContextData* getAudioContextData(Context& ctx) {
    auto it = ctx.state.find("__audio_context");
    if (it != ctx.state.end()) {
        return std::any_cast<AudioContextData*>(it->second);
    }
    return nullptr;
}

inline void setAudioContextData(Context& ctx, AudioContextData* data) {
    std::unique_lock lock(ctx.stateMutex);
    ctx.state["__audio_context"] = data;
}

} // namespace arxglue::sound