#include "generator_nodes.hpp"
#include <cmath>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace arxglue::sound::nodes {

// ========== SineOscillatorNode ==========
SineOscillatorNode::SineOscillatorNode() : m_frequency(440.0f), m_amplitude(0.5f) {}

SineOscillatorNode::SineOscillatorNode(float frequency, float amplitude)
    : m_frequency(frequency), m_amplitude(amplitude) {}

void SineOscillatorNode::prepare(const AudioSpec& spec) {
    m_sampleRate = spec.sampleRate;
    m_phaseIncrement = 2.0f * M_PI * m_frequency / m_sampleRate;
    m_lastSpec = spec;
    m_prepared = true;
}

void SineOscillatorNode::process(AudioBuffer& output) {
    if (!m_prepared) return;

    const size_t numFrames = output.getNumFrames();
    const uint16_t numChannels = output.getSpec().numChannels;
    float* data = output.data();

    for (size_t i = 0; i < numFrames; ++i) {
        float sample = m_amplitude * std::sin(m_phase);
        for (uint16_t ch = 0; ch < numChannels; ++ch) {
            *data++ = sample;
        }
        m_phase += m_phaseIncrement;
        if (m_phase > 2.0f * M_PI) {
            m_phase -= 2.0f * M_PI;
        }
    }
}

void SineOscillatorNode::reset() {
    m_phase = 0.0f;
}

ComponentMetadata SineOscillatorNode::getMetadata() const {
    return {
        "SineOscillator",
        {},
        {{"audio", typeid(std::shared_ptr<AudioBuffer>)}},
        false,
        false
    };
}

void SineOscillatorNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "frequency") {
        m_frequency = std::any_cast<float>(value);
        if (m_prepared) {
            m_phaseIncrement = 2.0f * M_PI * m_frequency / m_sampleRate;
        }
        setDirty(true);
    } else if (name == "amplitude") {
        m_amplitude = std::any_cast<float>(value);
        setDirty(true);
    }
}

std::any SineOscillatorNode::getParameter(const std::string& name) const {
    if (name == "frequency") return m_frequency;
    if (name == "amplitude") return m_amplitude;
    return {};
}

void SineOscillatorNode::serialize(nlohmann::json& j) const {
    j["type"] = "SineOscillator";
    j["params"]["frequency"] = m_frequency;
    j["params"]["amplitude"] = m_amplitude;
}

void SineOscillatorNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        if (j["params"].contains("frequency")) m_frequency = j["params"]["frequency"].get<float>();
        if (j["params"].contains("amplitude")) m_amplitude = j["params"]["amplitude"].get<float>();
    }
}

size_t SineOscillatorNode::computeParamsHash() const {
    size_t h1 = std::hash<float>{}(m_frequency);
    size_t h2 = std::hash<float>{}(m_amplitude);
    return h1 ^ (h2 << 1);
}

// ========== WhiteNoiseNode ==========
WhiteNoiseNode::WhiteNoiseNode() : m_amplitude(0.5f), m_rng(std::random_device{}()), m_dist(-1.0f, 1.0f) {}

WhiteNoiseNode::WhiteNoiseNode(float amplitude)
    : m_amplitude(amplitude), m_rng(std::random_device{}()), m_dist(-1.0f, 1.0f) {}

void WhiteNoiseNode::prepare(const AudioSpec& spec) {
    m_lastSpec = spec;
    m_prepared = true;
}

void WhiteNoiseNode::process(AudioBuffer& output) {
    if (!m_prepared) return;

    const size_t numSamples = output.getNumSamples();
    float* data = output.data();
    for (size_t i = 0; i < numSamples; ++i) {
        data[i] = m_amplitude * m_dist(m_rng);
    }
}

void WhiteNoiseNode::reset() {}

ComponentMetadata WhiteNoiseNode::getMetadata() const {
    return {
        "WhiteNoise",
        {},
        {{"audio", typeid(std::shared_ptr<AudioBuffer>)}},
        false,
        true
    };
}

void WhiteNoiseNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "amplitude") {
        m_amplitude = std::any_cast<float>(value);
        setDirty(true);
    }
}

std::any WhiteNoiseNode::getParameter(const std::string& name) const {
    if (name == "amplitude") return m_amplitude;
    return {};
}

void WhiteNoiseNode::serialize(nlohmann::json& j) const {
    j["type"] = "WhiteNoise";
    j["params"]["amplitude"] = m_amplitude;
}

void WhiteNoiseNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params") && j["params"].contains("amplitude")) {
        m_amplitude = j["params"]["amplitude"].get<float>();
    }
}

size_t WhiteNoiseNode::computeParamsHash() const {
    return std::hash<float>{}(m_amplitude);
}

} // namespace arxglue::sound::nodes