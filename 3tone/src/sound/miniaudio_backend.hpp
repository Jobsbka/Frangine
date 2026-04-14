#pragma once
#include "audio_backend.hpp"
#include "miniaudio.h"

namespace arxglue::sound {

class MiniAudioBackend : public AudioBackend {
public:
    MiniAudioBackend();
    ~MiniAudioBackend() override;

    std::vector<AudioDeviceInfo> enumerateDevices() override;
    std::unique_ptr<AudioDevice> openDevice(
        const AudioDeviceInfo* inputDevice,
        const AudioDeviceInfo* outputDevice,
        unsigned int sampleRate,
        unsigned int bufferSize,
        AudioCallback callback,
        void* userData) override;

    void shutdown() override;
    const char* getName() const override { return "MiniAudio"; }

private:
    ma_context m_context;
    bool m_initialized = false;
};

} // namespace arxglue::sound