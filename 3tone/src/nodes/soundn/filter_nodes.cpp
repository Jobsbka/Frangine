#include "filter_nodes.hpp"
#include <stdexcept>

namespace arxglue::sound::nodes {

// ========== BiquadFilterNode ==========
BiquadFilterNode::BiquadFilterNode()
    : m_type(dsp::BiquadType::LowPass), m_frequency(1000.0f), m_q(0.707f), m_gainDB(0.0f) {}

BiquadFilterNode::BiquadFilterNode(dsp::BiquadType type, float freq, float q, float gainDB)
    : m_type(type), m_frequency(freq), m_q(q), m_gainDB(gainDB) {}

void BiquadFilterNode::prepare(const AudioSpec& spec) {
    m_lastSpec = spec;
    m_filters.resize(spec.numChannels);
    updateFilters();
    m_prepared = true;
}

void BiquadFilterNode::updateFilters() {
    if (!m_prepared) return;
    for (auto& filter : m_filters) {
        filter.setParameters(m_type, m_frequency, m_q, m_gainDB, m_lastSpec.sampleRate);
    }
}

void BiquadFilterNode::process(AudioBuffer& output) {
    if (!m_prepared) return;
    const uint16_t numChannels = output.getSpec().numChannels;
    const size_t numFrames = output.getNumFrames();
    for (uint16_t ch = 0; ch < numChannels; ++ch) {
        float* channelData = output.getChannelData(ch);
        for (size_t i = 0; i < numFrames; ++i) {
            // Обрабатываем сэмпл, учитывая чередование
            size_t idx = i * numChannels + ch;
            output.data()[idx] = m_filters[ch].process(output.data()[idx]);
        }
    }
}

void BiquadFilterNode::reset() {
    for (auto& filter : m_filters) {
        filter.reset();
    }
}

ComponentMetadata BiquadFilterNode::getMetadata() const {
    return {
        "BiquadFilter",
        {{"audio", typeid(std::shared_ptr<AudioBuffer>)}},
        {{"audio", typeid(std::shared_ptr<AudioBuffer>)}},
        true,  // pure (детерминирован)
        false
    };
}

void BiquadFilterNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "type") {
        m_type = std::any_cast<dsp::BiquadType>(value);
        updateFilters();
        setDirty(true);
    } else if (name == "frequency") {
        m_frequency = std::any_cast<float>(value);
        updateFilters();
        setDirty(true);
    } else if (name == "q") {
        m_q = std::any_cast<float>(value);
        updateFilters();
        setDirty(true);
    } else if (name == "gain") {
        m_gainDB = std::any_cast<float>(value);
        updateFilters();
        setDirty(true);
    }
}

std::any BiquadFilterNode::getParameter(const std::string& name) const {
    if (name == "type") return m_type;
    if (name == "frequency") return m_frequency;
    if (name == "q") return m_q;
    if (name == "gain") return m_gainDB;
    return {};
}

void BiquadFilterNode::serialize(nlohmann::json& j) const {
    j["type"] = "BiquadFilter";
    j["params"]["filterType"] = static_cast<int>(m_type);
    j["params"]["frequency"] = m_frequency;
    j["params"]["q"] = m_q;
    j["params"]["gainDB"] = m_gainDB;
}

void BiquadFilterNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        if (j["params"].contains("filterType")) m_type = static_cast<dsp::BiquadType>(j["params"]["filterType"].get<int>());
        if (j["params"].contains("frequency")) m_frequency = j["params"]["frequency"].get<float>();
        if (j["params"].contains("q")) m_q = j["params"]["q"].get<float>();
        if (j["params"].contains("gainDB")) m_gainDB = j["params"]["gainDB"].get<float>();
    }
}

size_t BiquadFilterNode::computeParamsHash() const {
    size_t h1 = std::hash<int>{}(static_cast<int>(m_type));
    size_t h2 = std::hash<float>{}(m_frequency);
    size_t h3 = std::hash<float>{}(m_q);
    size_t h4 = std::hash<float>{}(m_gainDB);
    return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
}

// ========== GainNode ==========
GainNode::GainNode() : m_gain(1.0f) {}
GainNode::GainNode(float gain) : m_gain(gain) {}

void GainNode::prepare(const AudioSpec& spec) {
    m_lastSpec = spec;
    m_prepared = true;
}

void GainNode::process(AudioBuffer& output) {
    if (!m_prepared) return;
    const size_t numSamples = output.getNumSamples();
    float* data = output.data();
    for (size_t i = 0; i < numSamples; ++i) {
        data[i] *= m_gain;
    }
}

void GainNode::reset() {}

ComponentMetadata GainNode::getMetadata() const {
    return {
        "Gain",
        {{"audio", typeid(std::shared_ptr<AudioBuffer>)}},
        {{"audio", typeid(std::shared_ptr<AudioBuffer>)}},
        true,
        false
    };
}

void GainNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "gain") {
        m_gain = std::any_cast<float>(value);
        setDirty(true);
    }
}

std::any GainNode::getParameter(const std::string& name) const {
    if (name == "gain") return m_gain;
    return {};
}

void GainNode::serialize(nlohmann::json& j) const {
    j["type"] = "Gain";
    j["params"]["gain"] = m_gain;
}

void GainNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params") && j["params"].contains("gain")) {
        m_gain = j["params"]["gain"].get<float>();
    }
}

size_t GainNode::computeParamsHash() const {
    return std::hash<float>{}(m_gain);
}

} // namespace arxglue::sound::nodes