#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio_backend.hpp"
#include "audio_device.hpp"
#include <stdexcept>
#include <cstring>

namespace arxglue::sound {

class MiniAudioDevice : public AudioDevice {
public:
    MiniAudioDevice(ma_context* context,
                    const ma_device_config& config,
                    AudioCallback callback,
                    void* userData)
        : m_callback(std::move(callback)), m_userData(userData)
    {
        ma_result result = ma_device_init(context, &config, &m_device);
        if (result != MA_SUCCESS) {
            char errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg), "Failed to initialize miniaudio device. Error code: %d", result);
            throw std::runtime_error(errorMsg);
        }
        m_device.pUserData = this;
    }

    ~MiniAudioDevice() {
        ma_device_uninit(&m_device);
    }

    void start() override {
        if (ma_device_start(&m_device) != MA_SUCCESS) {
            throw std::runtime_error("Failed to start miniaudio device");
        }
        m_running = true;
    }

    void stop() override {
        if (ma_device_stop(&m_device) != MA_SUCCESS) {
            throw std::runtime_error("Failed to stop miniaudio device");
        }
        m_running = false;
    }

    bool isRunning() const override { return m_running; }
    double getSampleRate() const override { return m_device.sampleRate; }
    int getBufferSize() const override {
        return static_cast<int>(static_cast<ma_uint32>(m_device.playback.internalPeriodSizeInFrames));
    }
    int getNumOutputChannels() const override { return static_cast<int>(m_device.playback.channels); }

    static void dataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
        auto* self = static_cast<MiniAudioDevice*>(pDevice->pUserData);
        if (!self->m_callback) return;

        float** out = reinterpret_cast<float**>(&pOutput);
        float** in  = const_cast<float**>(reinterpret_cast<const float**>(&pInput));

        self->m_callback(out, in, static_cast<int>(frameCount), static_cast<int>(pDevice->playback.channels));
    }

private:
    ma_device m_device;
    AudioCallback m_callback;
    void* m_userData = nullptr;
    bool m_running = false;
};

// MiniAudioBackend
MiniAudioBackend::MiniAudioBackend() {
    if (ma_context_init(nullptr, 0, nullptr, &m_context) != MA_SUCCESS) {
        throw std::runtime_error("Failed to initialize miniaudio context");
    }
    m_initialized = true;
}

MiniAudioBackend::~MiniAudioBackend() {
    shutdown();
}

std::vector<AudioDeviceInfo> MiniAudioBackend::enumerateDevices() {
    std::vector<AudioDeviceInfo> devices;
    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info* pCaptureInfos;
    ma_uint32 captureCount;

    if (ma_context_get_devices(&m_context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) != MA_SUCCESS) {
        return devices;
    }

    for (ma_uint32 i = 0; i < playbackCount; ++i) {
        AudioDeviceInfo info;
        info.name = pPlaybackInfos[i].name;
        info.maxOutputChannels = (pPlaybackInfos[i].nativeDataFormatCount > 0)
                                 ? static_cast<int>(pPlaybackInfos[i].nativeDataFormats[0].channels)
                                 : 2;
        info.maxInputChannels = 0;
        info.isDefaultOutput = (pPlaybackInfos[i].isDefault != MA_FALSE);
        info.isDefaultInput = false;
        info.id = i;
        devices.push_back(info);
    }

    for (ma_uint32 i = 0; i < captureCount; ++i) {
        AudioDeviceInfo info;
        info.name = pCaptureInfos[i].name;
        info.maxInputChannels = (pCaptureInfos[i].nativeDataFormatCount > 0)
                                ? static_cast<int>(pCaptureInfos[i].nativeDataFormats[0].channels)
                                : 2;
        info.maxOutputChannels = 0;
        info.isDefaultInput = (pCaptureInfos[i].isDefault != MA_FALSE);
        info.isDefaultOutput = false;
        info.id = playbackCount + i;
        devices.push_back(info);
    }

    return devices;
}

std::unique_ptr<AudioDevice> MiniAudioBackend::openDevice(
    const AudioDeviceInfo* inputDevice,
    const AudioDeviceInfo* outputDevice,
    unsigned int sampleRate,
    unsigned int bufferSize,
    AudioCallback callback,
    void* userData)
{
    ma_device_config config = ma_device_config_init(ma_device_type_playback);

    if (outputDevice) {
        config.playback.pDeviceID = reinterpret_cast<const ma_device_id*>(&outputDevice->id);
        config.playback.format   = ma_format_f32;
        config.playback.channels = (outputDevice->maxOutputChannels > 0)
                                   ? static_cast<ma_uint32>(outputDevice->maxOutputChannels)
                                   : 0;
    } else {
        config.playback.pDeviceID = nullptr;
        config.playback.format    = ma_format_f32;
        config.playback.channels  = 0;
    }

    if (inputDevice) {
        config.capture.pDeviceID = reinterpret_cast<const ma_device_id*>(&inputDevice->id);
        config.capture.format    = ma_format_f32;
        config.capture.channels  = (inputDevice->maxInputChannels > 0)
                                   ? static_cast<ma_uint32>(inputDevice->maxInputChannels)
                                   : 0;
    }

    config.sampleRate      = (sampleRate > 0) ? sampleRate : 0;
    config.periodSizeInFrames = bufferSize;
    config.dataCallback    = MiniAudioDevice::dataCallback;
    config.pUserData       = this;

    return std::make_unique<MiniAudioDevice>(&m_context, config, std::move(callback), userData);
}

void MiniAudioBackend::shutdown() {
    if (m_initialized) {
        ma_context_uninit(&m_context);
        m_initialized = false;
    }
}

} // namespace arxglue::sound