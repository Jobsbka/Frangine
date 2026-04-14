#include "io_nodes.hpp"
#include "../../sound/audio_asset.hpp"
#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace arxglue::sound::nodes {

// ========== AudioOutputNode ==========
AudioOutputNode::AudioOutputNode() = default;

AudioOutputNode::AudioOutputNode(std::shared_ptr<AudioDevice> device)
    : m_device(std::move(device)) {}

AudioOutputNode::~AudioOutputNode() {
    if (m_device && m_deviceStarted) {
        m_device->stop();
    }
}

void AudioOutputNode::prepare(const AudioSpec& spec) {
    m_lastSpec = spec;
    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        m_outputBuffer = AudioBuffer(spec, spec.blockSize);
        m_outputBuffer.clear();
    }
    m_prepared = true;
}

void AudioOutputNode::process(AudioBuffer& output) {
    if (!m_prepared) return;

    // Копируем входные данные в локальный буфер под защитой мьютекса
    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        if (output.getNumFrames() != m_outputBuffer.getNumFrames() ||
            output.getSpec() != m_outputBuffer.getSpec()) {
            m_outputBuffer = AudioBuffer(output.getSpec(), output.getNumFrames());
        }
        std::copy_n(output.data(), output.getNumSamples(), m_outputBuffer.data());
    }
    m_hasNewData = true;

    // Запускаем устройство при первой необходимости
    if (m_device && !m_deviceStarted) {
        m_device->start();
        m_deviceStarted = true;
    }
}

void AudioOutputNode::audioCallback(float** out, float** /*in*/, int numFrames, int numChannels) {
    if (!m_prepared) {
        // Тишина
        for (int ch = 0; ch < numChannels; ++ch) {
            std::fill_n(out[ch], numFrames, 0.0f);
        }
        return;
    }

    std::lock_guard<std::mutex> lock(m_bufferMutex);
    if (!m_hasNewData) {
        for (int ch = 0; ch < numChannels; ++ch) {
            std::fill_n(out[ch], numFrames, 0.0f);
        }
        return;
    }

    // Копируем данные из m_outputBuffer в выходной буфер устройства
    size_t framesToCopy = std::min(static_cast<size_t>(numFrames), m_outputBuffer.getNumFrames());
    const float* src = m_outputBuffer.data();
    for (int ch = 0; ch < numChannels; ++ch) {
        float* dest = out[ch];
        for (size_t i = 0; i < framesToCopy; ++i) {
            dest[i] = src[i * numChannels + ch];
        }
        if (static_cast<size_t>(numFrames) > framesToCopy) {
            std::fill_n(dest + framesToCopy, numFrames - framesToCopy, 0.0f);
        }
    }
    m_hasNewData = false;
}

void AudioOutputNode::reset() {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    m_outputBuffer.clear();
    m_hasNewData = false;
}

ComponentMetadata AudioOutputNode::getMetadata() const {
    return {
        "AudioOutput",
        {{"audio", typeid(std::shared_ptr<AudioBuffer>)}},
        {},
        false,
        false
    };
}

void AudioOutputNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "device") {
        m_device = std::any_cast<std::shared_ptr<AudioDevice>>(value);
    }
}

std::any AudioOutputNode::getParameter(const std::string& name) const {
    if (name == "device") return m_device;
    return {};
}

void AudioOutputNode::serialize(nlohmann::json& j) const {
    j["type"] = "AudioOutput";
}

void AudioOutputNode::deserialize(const nlohmann::json& j) {
    (void)j;
}

// ========== AudioFileWriterNode ==========
AudioFileWriterNode::AudioFileWriterNode() = default;
AudioFileWriterNode::AudioFileWriterNode(const std::string& path) : m_path(path) {}

void AudioFileWriterNode::prepare(const AudioSpec& spec) {
    m_lastSpec = spec;
    m_accumBuffer = std::make_unique<AudioBuffer>(spec);
    m_framesWritten = 0;
    m_prepared = true;
}

void AudioFileWriterNode::process(AudioBuffer& output) {
    if (!m_prepared) return;

    size_t newFrames = output.getNumFrames();
    if (newFrames == 0) return;

    size_t oldSize = m_accumBuffer->getNumFrames();
    m_accumBuffer->resize(oldSize + newFrames, true);

    float* dest = m_accumBuffer->data() + oldSize * m_lastSpec.numChannels;
    const float* src = output.data();
    std::copy_n(src, newFrames * m_lastSpec.numChannels, dest);

    m_framesWritten += newFrames;
}

void AudioFileWriterNode::reset() {
    if (m_accumBuffer) {
        m_accumBuffer->clear();
        m_framesWritten = 0;
    }
}

ComponentMetadata AudioFileWriterNode::getMetadata() const {
    return {
        "AudioFileWriter",
        {{"audio", typeid(std::shared_ptr<AudioBuffer>)}},
        {},
        false,
        false
    };
}

void AudioFileWriterNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "path") {
        m_path = std::any_cast<std::string>(value);
    }
}

std::any AudioFileWriterNode::getParameter(const std::string& name) const {
    if (name == "path") return m_path;
    return {};
}

void AudioFileWriterNode::serialize(nlohmann::json& j) const {
    j["type"] = "AudioFileWriter";
    j["params"]["path"] = m_path;
}

void AudioFileWriterNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params") && j["params"].contains("path")) {
        m_path = j["params"]["path"].get<std::string>();
    }
}

// ========== AudioFileReaderNode ==========
AudioFileReaderNode::AudioFileReaderNode() = default;
AudioFileReaderNode::AudioFileReaderNode(const std::string& path) : m_path(path) {}

void AudioFileReaderNode::prepare(const AudioSpec& spec) {
    m_lastSpec = spec;
    if (!m_path.empty()) {
        m_asset = AudioAsset::loadFromFile(m_path);
    }
    m_playhead = 0;
    m_prepared = true;
}

void AudioFileReaderNode::process(AudioBuffer& output) {
    if (!m_prepared || !m_asset) {
        output.clear();
        return;
    }

    const AudioBuffer* source = m_asset->getBuffer();
    if (!source) {
        output.clear();
        return;
    }

    size_t srcFrames = source->getNumFrames();
    size_t outFrames = output.getNumFrames();
    const uint16_t srcChannels = source->getSpec().numChannels;
    const uint16_t outChannels = output.getSpec().numChannels;

    for (size_t i = 0; i < outFrames; ++i) {
        if (m_playhead >= srcFrames) {
            if (m_loop) {
                m_playhead = 0;
            } else {
                for (uint16_t ch = 0; ch < outChannels; ++ch) {
                    output.getChannelData(ch)[i] = 0.0f;
                }
                continue;
            }
        }

        for (uint16_t ch = 0; ch < outChannels; ++ch) {
            uint16_t srcCh = (ch < srcChannels) ? ch : 0;
            float sample = source->getChannelData(srcCh)[m_playhead * srcChannels + srcCh];
            output.getChannelData(ch)[i] = sample;
        }
        ++m_playhead;
    }
}

void AudioFileReaderNode::reset() {
    m_playhead = 0;
}

ComponentMetadata AudioFileReaderNode::getMetadata() const {
    return {
        "AudioFileReader",
        {},
        {{"audio", typeid(std::shared_ptr<AudioBuffer>)}},
        false,
        false
    };
}

void AudioFileReaderNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "path") {
        m_path = std::any_cast<std::string>(value);
        m_asset.reset();
    } else if (name == "loop") {
        m_loop = std::any_cast<bool>(value);
    }
}

std::any AudioFileReaderNode::getParameter(const std::string& name) const {
    if (name == "path") return m_path;
    if (name == "loop") return m_loop;
    return {};
}

void AudioFileReaderNode::serialize(nlohmann::json& j) const {
    j["type"] = "AudioFileReader";
    j["params"]["path"] = m_path;
    j["params"]["loop"] = m_loop;
}

void AudioFileReaderNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        if (j["params"].contains("path")) m_path = j["params"]["path"].get<std::string>();
        if (j["params"].contains("loop")) m_loop = j["params"]["loop"].get<bool>();
    }
}

} // namespace arxglue::sound::nodes