#pragma once
#include "../assets/asset_manager.hpp"
#include "audio_buffer.hpp"
#include <string>
#include <memory>
#include <vector>
#include <typeindex>

namespace arxglue::sound {

class AudioAsset : public Asset {
public:
    AudioAsset();
    explicit AudioAsset(std::unique_ptr<AudioBuffer> buffer);
    ~AudioAsset() override = default;

    static std::shared_ptr<AudioAsset> loadFromFile(const std::string& path);
    bool saveToFile(const std::string& path) const;

    const AudioBuffer* getBuffer() const { return m_buffer.get(); }
    AudioBuffer* getBuffer() { return m_buffer.get(); }

    void setBuffer(std::unique_ptr<AudioBuffer> buffer);

    std::type_index getType() const override { return typeid(AudioAsset); }
    void writeToFile(const std::string& path) const override;

    // Дополнительные метаданные
    void setLoopPoints(size_t start, size_t end);
    std::pair<size_t, size_t> getLoopPoints() const { return {m_loopStart, m_loopEnd}; }

    void setTempo(double bpm) { m_tempo = bpm; }
    double getTempo() const { return m_tempo; }

private:
    std::unique_ptr<AudioBuffer> m_buffer;
    size_t m_loopStart = 0;
    size_t m_loopEnd = 0;
    double m_tempo = 120.0;

    bool loadWav(const std::string& path);
    bool saveWav(const std::string& path) const;
};

} // namespace arxglue::sound