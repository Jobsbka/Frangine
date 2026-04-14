#pragma once
#include "../../sound/audio_node.hpp"
#include <vector>

namespace arxglue::sound::nodes {

class MixerNode : public AudioNode {
public:
    MixerNode();
    explicit MixerNode(int numInputs);

    void prepare(const AudioSpec& spec) override;
    void process(AudioBuffer& output) override;
    void reset() override;

    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

    // Специфичный метод для MixerNode: установка усиления конкретного входа
    void setGain(int inputIndex, float gain);

protected:
    size_t computeParamsHash() const override;

private:
    int m_numInputs;
    std::vector<float> m_gains;
    std::vector<AudioBuffer> m_inputBuffers; // временные буферы для входов
};

class PannerNode : public AudioNode {
public:
    PannerNode();
    explicit PannerNode(float pan);

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
    float m_pan; // -1.0 = left, 0.0 = center, 1.0 = right
};

} // namespace arxglue::sound::nodes