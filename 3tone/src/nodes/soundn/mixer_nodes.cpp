#include "mixer_nodes.hpp"
#include <stdexcept>
#include <algorithm>
#include <cmath>

namespace arxglue::sound::nodes {

// ========== MixerNode ==========
MixerNode::MixerNode() : m_numInputs(2) {
    m_gains.resize(m_numInputs, 1.0f);
}

MixerNode::MixerNode(int numInputs) : m_numInputs(numInputs) {
    if (numInputs < 1) m_numInputs = 1;
    m_gains.resize(m_numInputs, 1.0f);
}

void MixerNode::prepare(const AudioSpec& spec) {
    m_lastSpec = spec;
    m_inputBuffers.clear();
    for (int i = 0; i < m_numInputs; ++i) {
        m_inputBuffers.emplace_back(spec);
    }
    m_prepared = true;
}

void MixerNode::process(AudioBuffer& output) {
    if (!m_prepared) return;

    // Предполагаем, что входные данные были помещены в контекст с ключами "in_0", "in_1", ...
    // В реализации execute() базового класса это уже должно быть сделано.
    // Здесь мы просто получаем входные буферы через getInputValue() или state.
    // Для простоты будем считать, что в m_inputBuffers уже лежат данные (заполняются в execute).

    const size_t numSamples = output.getNumSamples();
    output.clear();

    for (int i = 0; i < m_numInputs; ++i) {
        const float* inputData = m_inputBuffers[i].data();
        if (!inputData) continue;
        float gain = m_gains[i];
        float* outData = output.data();
        for (size_t j = 0; j < numSamples; ++j) {
            outData[j] += inputData[j] * gain;
        }
    }
}

void MixerNode::reset() {
    for (auto& buf : m_inputBuffers) {
        buf.clear();
    }
}

ComponentMetadata MixerNode::getMetadata() const {
    std::vector<PortInfo> inputs;
    for (int i = 0; i < m_numInputs; ++i) {
        inputs.push_back({"in" + std::to_string(i), typeid(std::shared_ptr<AudioBuffer>), true});
    }
    return {
        "Mixer",
        inputs,
        {{"audio", typeid(std::shared_ptr<AudioBuffer>)}},
        true,
        false
    };
}

void MixerNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "numInputs") {
        int newNum = std::any_cast<int>(value);
        if (newNum < 1) newNum = 1;
        if (newNum != m_numInputs) {
            m_numInputs = newNum;
            m_gains.resize(m_numInputs, 1.0f);
            if (m_prepared) {
                m_inputBuffers.clear();
                for (int i = 0; i < m_numInputs; ++i) {
                    m_inputBuffers.emplace_back(m_lastSpec);
                }
            }
            setDirty(true);
        }
    } else if (name.rfind("gain", 0) == 0) {
        // Параметр вида "gain0", "gain1" и т.д.
        int idx = std::stoi(name.substr(4));
        if (idx >= 0 && idx < m_numInputs) {
            m_gains[idx] = std::any_cast<float>(value);
            setDirty(true);
        }
    }
}

std::any MixerNode::getParameter(const std::string& name) const {
    if (name == "numInputs") return m_numInputs;
    if (name.rfind("gain", 0) == 0) {
        int idx = std::stoi(name.substr(4));
        if (idx >= 0 && idx < m_numInputs) return m_gains[idx];
    }
    return {};
}

void MixerNode::setGain(int inputIndex, float gain) {
    if (inputIndex >= 0 && inputIndex < m_numInputs) {
        m_gains[inputIndex] = gain;
        setDirty(true);
    }
}

void MixerNode::serialize(nlohmann::json& j) const {
    j["type"] = "Mixer";
    j["params"]["numInputs"] = m_numInputs;
    nlohmann::json gainsArr = nlohmann::json::array();
    for (float g : m_gains) gainsArr.push_back(g);
    j["params"]["gains"] = gainsArr;
}

void MixerNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        if (j["params"].contains("numInputs")) {
            m_numInputs = j["params"]["numInputs"].get<int>();
            if (m_numInputs < 1) m_numInputs = 1;
        }
        m_gains.resize(m_numInputs, 1.0f);
        if (j["params"].contains("gains")) {
            auto gainsArr = j["params"]["gains"];
            for (size_t i = 0; i < gainsArr.size() && i < (size_t)m_numInputs; ++i) {
                m_gains[i] = gainsArr[i].get<float>();
            }
        }
    }
}

size_t MixerNode::computeParamsHash() const {
    size_t h = std::hash<int>{}(m_numInputs);
    for (float g : m_gains) {
        h ^= std::hash<float>{}(g) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    return h;
}

// ========== PannerNode ==========
PannerNode::PannerNode() : m_pan(0.0f) {}
PannerNode::PannerNode(float pan) : m_pan(std::clamp(pan, -1.0f, 1.0f)) {}

void PannerNode::prepare(const AudioSpec& spec) {
    m_lastSpec = spec;
    m_prepared = true;
}

void PannerNode::process(AudioBuffer& output) {
    if (!m_prepared) return;

    const uint16_t numChannels = output.getSpec().numChannels;
    if (numChannels < 2) return; // панорамирование только для стерео и более

    const size_t numFrames = output.getNumFrames();
    float* data = output.data();

    // Расчёт коэффициентов (constant power pan)
    float leftGain = std::sqrt(0.5f * (1.0f - m_pan));
    float rightGain = std::sqrt(0.5f * (1.0f + m_pan));

    for (size_t i = 0; i < numFrames; ++i) {
        size_t baseIdx = i * numChannels;
        // Для простоты предполагаем, что входной сигнал находится в первом канале (моно)
        // или в каналах 0 и 1 для стерео.
        // Здесь реализуем преобразование моно->стерео: если на входе моно, панорамируем его.
        // Если стерео - балансируем.
        float leftSample = data[baseIdx];
        float rightSample = (numChannels > 1) ? data[baseIdx + 1] : leftSample;

        data[baseIdx] = leftSample * leftGain;
        if (numChannels > 1) {
            data[baseIdx + 1] = rightSample * rightGain;
        }
        // Остальные каналы не трогаем
    }
}

void PannerNode::reset() {}

ComponentMetadata PannerNode::getMetadata() const {
    return {
        "Panner",
        {{"audio", typeid(std::shared_ptr<AudioBuffer>)}},
        {{"audio", typeid(std::shared_ptr<AudioBuffer>)}},
        true,
        false
    };
}

void PannerNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "pan") {
        m_pan = std::clamp(std::any_cast<float>(value), -1.0f, 1.0f);
        setDirty(true);
    }
}

std::any PannerNode::getParameter(const std::string& name) const {
    if (name == "pan") return m_pan;
    return {};
}

void PannerNode::serialize(nlohmann::json& j) const {
    j["type"] = "Panner";
    j["params"]["pan"] = m_pan;
}

void PannerNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params") && j["params"].contains("pan")) {
        m_pan = std::clamp(j["params"]["pan"].get<float>(), -1.0f, 1.0f);
    }
}

size_t PannerNode::computeParamsHash() const {
    return std::hash<float>{}(m_pan);
}

} // namespace arxglue::sound::nodes