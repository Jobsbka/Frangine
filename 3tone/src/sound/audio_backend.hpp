#pragma once
#include <vector>
#include <string>
#include <functional>
#include <memory>

namespace arxglue::sound {

struct AudioDeviceInfo {
    std::string name;
    int maxInputChannels;
    int maxOutputChannels;
    bool isDefaultInput;
    bool isDefaultOutput;
    unsigned int id; // внутренний идентификатор бэкенда
};

class AudioDevice;

using AudioCallback = std::function<void(float** output, float** input,
                                         int numFrames, int numChannels)>;

class AudioBackend {
public:
    virtual ~AudioBackend() = default;

    virtual std::vector<AudioDeviceInfo> enumerateDevices() = 0;
    virtual std::unique_ptr<AudioDevice> openDevice(
        const AudioDeviceInfo* inputDevice,
        const AudioDeviceInfo* outputDevice,
        unsigned int sampleRate,
        unsigned int bufferSize,
        AudioCallback callback,
        void* userData) = 0;

    virtual void shutdown() = 0;
    virtual const char* getName() const = 0;
};

} // namespace arxglue::sound