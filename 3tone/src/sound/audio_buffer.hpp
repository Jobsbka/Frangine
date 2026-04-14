#pragma once
#include "audio_spec.hpp"
#include <vector>
#include <cstring>
#include <stdexcept>

namespace arxglue::sound {

class AudioBuffer {
public:
    AudioBuffer() = default;

    AudioBuffer(const AudioSpec& spec, size_t numFrames = 0)
        : m_spec(spec), m_numFrames(numFrames)
    {
        if (numFrames > 0) {
            m_data.resize(numFrames * spec.numChannels, 0.0f);
        }
    }

    ~AudioBuffer() = default;

    // Доступ к данным канала
    float* getChannelData(uint32_t channel) {
        if (channel >= m_spec.numChannels) {
            throw std::out_of_range("AudioBuffer::getChannelData: channel index out of range");
        }
        return m_data.data() + channel;
    }

    const float* getChannelData(uint32_t channel) const {
        if (channel >= m_spec.numChannels) {
            throw std::out_of_range("AudioBuffer::getChannelData: channel index out of range");
        }
        return m_data.data() + channel;
    }

    // Доступ к сырым данным (чередование каналов)
    float* data() { return m_data.data(); }
    const float* data() const { return m_data.data(); }

    // Очистка буфера (заполнение нулями)
    void clear() {
        std::fill(m_data.begin(), m_data.end(), 0.0f);
    }

    // Свойства
    const AudioSpec& getSpec() const { return m_spec; }
    size_t getNumFrames() const { return m_numFrames; }
    size_t getNumSamples() const { return m_numFrames * m_spec.numChannels; }

    // Изменение размера
    void resize(size_t newFrames, bool keepData = false) {
        if (newFrames == m_numFrames) return;
        size_t newSize = newFrames * m_spec.numChannels;
        if (keepData) {
            std::vector<float> newData(newSize, 0.0f);
            size_t copyFrames = std::min(m_numFrames, newFrames);
            if (copyFrames > 0) {
                size_t copySamples = copyFrames * m_spec.numChannels;
                std::copy_n(m_data.begin(), copySamples, newData.begin());
            }
            m_data.swap(newData);
        } else {
            m_data.resize(newSize, 0.0f);
        }
        m_numFrames = newFrames;
    }

    // Изменение спецификации (с потерей данных)
    void setSpec(const AudioSpec& spec) {
        if (spec == m_spec) return;
        m_spec = spec;
        m_numFrames = 0;
        m_data.clear();
    }

    // Копирование запрещено, перемещение разрешено
    AudioBuffer(const AudioBuffer&) = delete;
    AudioBuffer& operator=(const AudioBuffer&) = delete;

    AudioBuffer(AudioBuffer&& other) noexcept
        : m_spec(other.m_spec)
        , m_numFrames(other.m_numFrames)
        , m_data(std::move(other.m_data))
    {
        other.m_numFrames = 0;
    }

    AudioBuffer& operator=(AudioBuffer&& other) noexcept {
        if (this != &other) {
            m_spec = other.m_spec;
            m_numFrames = other.m_numFrames;
            m_data = std::move(other.m_data);
            other.m_numFrames = 0;
        }
        return *this;
    }

private:
    AudioSpec m_spec;
    size_t m_numFrames = 0;
    std::vector<float> m_data; // плоский массив, каналы чередуются
};

} // namespace arxglue::sound