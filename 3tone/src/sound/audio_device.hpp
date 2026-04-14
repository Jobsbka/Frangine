#pragma once
#include <cstdint>
#include <memory>

namespace arxglue::sound {

class AudioDevice {
public:
    virtual ~AudioDevice() = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual double getSampleRate() const = 0;
    virtual int getBufferSize() const = 0;
    virtual int getNumOutputChannels() const = 0;
};

} // namespace arxglue::sound