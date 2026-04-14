#include "audio_asset.hpp"
#include <fstream>
#include <cstring>
#include <stdexcept>

namespace arxglue::sound {

#pragma pack(push, 1)
struct WavHeader {
    char chunkId[4];        // "RIFF"
    uint32_t chunkSize;
    char format[4];         // "WAVE"
    char subchunk1Id[4];    // "fmt "
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char subchunk2Id[4];    // "data"
    uint32_t subchunk2Size;
};
#pragma pack(pop)

AudioAsset::AudioAsset() = default;

AudioAsset::AudioAsset(std::unique_ptr<AudioBuffer> buffer)
    : m_buffer(std::move(buffer))
{}

void AudioAsset::setBuffer(std::unique_ptr<AudioBuffer> buffer) {
    m_buffer = std::move(buffer);
}

std::shared_ptr<AudioAsset> AudioAsset::loadFromFile(const std::string& path) {
    auto asset = std::make_shared<AudioAsset>();
    if (asset->loadWav(path)) {
        return asset;
    }
    return nullptr;
}

bool AudioAsset::saveToFile(const std::string& path) const {
    return saveWav(path);
}

void AudioAsset::writeToFile(const std::string& path) const {
    saveToFile(path);
}

void AudioAsset::setLoopPoints(size_t start, size_t end) {
    m_loopStart = start;
    m_loopEnd = end;
}

bool AudioAsset::loadWav(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    WavHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (std::strncmp(header.chunkId, "RIFF", 4) != 0 ||
        std::strncmp(header.format, "WAVE", 4) != 0 ||
        std::strncmp(header.subchunk1Id, "fmt ", 4) != 0) {
        return false;
    }

    // Поддерживаем только PCM 16-bit или float32
    if (header.audioFormat != 1 && header.audioFormat != 3) {
        return false; // PCM или IEEE float
    }

    AudioSpec spec;
    spec.sampleRate = header.sampleRate;
    spec.numChannels = header.numChannels;
    spec.blockSize = 512; // значение по умолчанию

    // Пропускаем возможные дополнительные данные в fmt чанке
    if (header.subchunk1Size > 16) {
        file.seekg(header.subchunk1Size - 16, std::ios::cur);
    }

    // Ищем data чанк
    char chunkId[4];
    uint32_t chunkSize;
    while (file.read(chunkId, 4)) {
        file.read(reinterpret_cast<char*>(&chunkSize), 4);
        if (std::strncmp(chunkId, "data", 4) == 0) {
            break;
        }
        file.seekg(chunkSize, std::ios::cur);
    }

    size_t numFrames = chunkSize / (header.numChannels * (header.bitsPerSample / 8));
    m_buffer = std::make_unique<AudioBuffer>(spec, numFrames);

    if (header.bitsPerSample == 16) {
        std::vector<int16_t> temp(numFrames * header.numChannels);
        file.read(reinterpret_cast<char*>(temp.data()), chunkSize);
        float* out = m_buffer->data();
        for (size_t i = 0; i < temp.size(); ++i) {
            out[i] = temp[i] / 32768.0f;
        }
    } else if (header.bitsPerSample == 32 && header.audioFormat == 3) {
        file.read(reinterpret_cast<char*>(m_buffer->data()), chunkSize);
    } else {
        return false;
    }

    m_loopEnd = numFrames;
    return true;
}

bool AudioAsset::saveWav(const std::string& path) const {
    if (!m_buffer) return false;

    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    const AudioSpec& spec = m_buffer->getSpec();
    size_t numFrames = m_buffer->getNumFrames();
    size_t dataSize = numFrames * spec.numChannels * sizeof(float);

    WavHeader header;
    std::memcpy(header.chunkId, "RIFF", 4);
    header.chunkSize = static_cast<uint32_t>(36 + dataSize);
    std::memcpy(header.format, "WAVE", 4);
    std::memcpy(header.subchunk1Id, "fmt ", 4);
    header.subchunk1Size = 16;
    header.audioFormat = 3; // IEEE float
    header.numChannels = spec.numChannels;
    header.sampleRate = spec.sampleRate;
    header.byteRate = spec.sampleRate * spec.numChannels * sizeof(float);
    header.blockAlign = spec.numChannels * sizeof(float);
    header.bitsPerSample = 32;
    std::memcpy(header.subchunk2Id, "data", 4);
    header.subchunk2Size = static_cast<uint32_t>(dataSize);

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(m_buffer->data()), dataSize);

    return true;
}

} // namespace arxglue::sound