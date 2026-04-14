#pragma once
#include "../../sound/audio_node.hpp"
#include "../../sound/ga_spatial.hpp"
#include <memory>

namespace arxglue::sound::nodes {

class GASpatializerNode : public AudioNode {
public:
    GASpatializerNode();
    ~GASpatializerNode() override = default;

    void prepare(const AudioSpec& spec) override;
    void process(AudioBuffer& output) override;
    void reset() override;

    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

private:
    std::unique_ptr<GASpatializer> m_spatializer;
    AudioBuffer m_leftBuffer;
    AudioBuffer m_rightBuffer;

    // Параметры, которые можно установить через setParameter
    std::array<float, 3> m_listenerPos = {0,0,0};
    std::array<float, 3> m_sourcePos = {0,0,0};
};

} // namespace arxglue::sound::nodes