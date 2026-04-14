#pragma once
#include "../../sound/audio_node.hpp"
#include "../../sound/dsp/biquad.hpp"
#include <vector>

namespace arxglue::sound::nodes {

class BiquadFilterNode : public AudioNode {
public:
    BiquadFilterNode();
    explicit BiquadFilterNode(dsp::BiquadType type, float freq, float q, float gainDB = 0.0f);

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
    dsp::BiquadType m_type;
    float m_frequency;
    float m_q;
    float m_gainDB;
    std::vector<dsp::BiquadFilter> m_filters; // по одному на канал

    void updateFilters();
};

class GainNode : public AudioNode {
public:
    GainNode();
    explicit GainNode(float gain);

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
    float m_gain;
};

} // namespace arxglue::sound::nodes