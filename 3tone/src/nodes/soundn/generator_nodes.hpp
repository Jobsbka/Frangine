#pragma once
#include "../../sound/audio_node.hpp"
#include <cmath>
#include <random>

namespace arxglue::sound::nodes {

class SineOscillatorNode : public AudioNode {
public:
    SineOscillatorNode();
    explicit SineOscillatorNode(float frequency, float amplitude = 0.5f);

    void prepare(const AudioSpec& spec) override;
    void process(AudioBuffer& output) override;
    void reset() override;

    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

protected:
    size_t computeParamsHash() const override;

private:
    float m_frequency;
    float m_amplitude;
    float m_phase = 0.0f;
    float m_phaseIncrement = 0.0f;
    uint32_t m_sampleRate = 0;
};

class WhiteNoiseNode : public AudioNode {
public:
    WhiteNoiseNode();
    explicit WhiteNoiseNode(float amplitude);

    void prepare(const AudioSpec& spec) override;
    void process(AudioBuffer& output) override;
    void reset() override;

    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

protected:
    size_t computeParamsHash() const override;

private:
    float m_amplitude;
    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_dist;
};

} // namespace arxglue::sound::nodes