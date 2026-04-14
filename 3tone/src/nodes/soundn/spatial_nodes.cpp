#include "spatial_nodes.hpp"
#include <stdexcept>

namespace arxglue::sound::nodes {

GASpatializerNode::GASpatializerNode()
    : m_spatializer(std::make_unique<GASpatializer>()) {}

void GASpatializerNode::prepare(const AudioSpec& spec) {
    m_lastSpec = spec;
    // Выход будет стерео, поэтому подготавливаем два моно-буфера
    AudioSpec monoSpect = spec;
    monoSpect.numChannels = 1;
    m_leftBuffer = AudioBuffer(monoSpect, spec.blockSize);
    m_rightBuffer = AudioBuffer(monoSpect, spec.blockSize);
    m_prepared = true;
}

void GASpatializerNode::process(AudioBuffer& output) {
    if (!m_prepared) return;

    // Ожидаем моно-вход (или берём первый канал)
    const uint16_t inChannels = output.getSpec().numChannels;
    if (inChannels == 0) return;

    // Если вход не моно, создаём временный моно-буфер, усредняя каналы
    AudioBuffer monoInput(m_lastSpec, output.getNumFrames());
    monoInput.clear();
    float* monoData = monoInput.data();
    const float* inData = output.data();
    size_t numFrames = output.getNumFrames();
    for (size_t i = 0; i < numFrames; ++i) {
        float sum = 0.0f;
        for (uint16_t ch = 0; ch < inChannels; ++ch) {
            sum += inData[i * inChannels + ch];
        }
        monoData[i] = sum / inChannels;
    }

    // Применяем пространственную обработку
    m_spatializer->processMonoToStereo(monoInput, m_leftBuffer, m_rightBuffer);

    // Копируем результат в выходной буфер (стерео)
    AudioSpec outSpec = output.getSpec();
    if (outSpec.numChannels >= 2) {
        float* outData = output.data();
        const float* leftData = m_leftBuffer.data();
        const float* rightData = m_rightBuffer.data();
        for (size_t i = 0; i < numFrames; ++i) {
            outData[i * outSpec.numChannels + 0] = leftData[i];
            outData[i * outSpec.numChannels + 1] = rightData[i];
            // Остальные каналы зануляем
            for (uint16_t ch = 2; ch < outSpec.numChannels; ++ch) {
                outData[i * outSpec.numChannels + ch] = 0.0f;
            }
        }
    } else {
        // Если выход моно, микшируем левый и правый
        output.clear();
        float* outData = output.data();
        const float* leftData = m_leftBuffer.data();
        const float* rightData = m_rightBuffer.data();
        for (size_t i = 0; i < numFrames; ++i) {
            outData[i] = (leftData[i] + rightData[i]) * 0.5f;
        }
    }
}

void GASpatializerNode::reset() {
    m_leftBuffer.clear();
    m_rightBuffer.clear();
}

ComponentMetadata GASpatializerNode::getMetadata() const {
    return {
        "GASpatializer",
        {{"audio", typeid(std::shared_ptr<AudioBuffer>)}},
        {{"audio", typeid(std::shared_ptr<AudioBuffer>)}},
        true,
        false
    };
}

void GASpatializerNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "listenerPos") {
        m_listenerPos = std::any_cast<std::array<float,3>>(value);
        m_spatializer->setListenerPosition(m_listenerPos);
        setDirty(true);
    } else if (name == "sourcePos") {
        m_sourcePos = std::any_cast<std::array<float,3>>(value);
        m_spatializer->setSourcePosition(m_sourcePos);
        setDirty(true);
    } else if (name == "listenerForward") {
        auto fwd = std::any_cast<std::array<float,3>>(value);
        m_spatializer->setListenerOrientation(fwd, {0,1,0});
        setDirty(true);
    } else if (name == "listenerUp") {
        auto up = std::any_cast<std::array<float,3>>(value);
        // Сохраняем текущий forward
        m_spatializer->setListenerOrientation({0,0,-1}, up);
        setDirty(true);
    }
}

std::any GASpatializerNode::getParameter(const std::string& name) const {
    if (name == "listenerPos") return m_listenerPos;
    if (name == "sourcePos") return m_sourcePos;
    return {};
}

void GASpatializerNode::serialize(nlohmann::json& j) const {
    j["type"] = "GASpatializer";
    j["params"]["listenerPos"] = {m_listenerPos[0], m_listenerPos[1], m_listenerPos[2]};
    j["params"]["sourcePos"] = {m_sourcePos[0], m_sourcePos[1], m_sourcePos[2]};
}

void GASpatializerNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        if (j["params"].contains("listenerPos")) {
            auto arr = j["params"]["listenerPos"];
            m_listenerPos = {arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>()};
            m_spatializer->setListenerPosition(m_listenerPos);
        }
        if (j["params"].contains("sourcePos")) {
            auto arr = j["params"]["sourcePos"];
            m_sourcePos = {arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>()};
            m_spatializer->setSourcePosition(m_sourcePos);
        }
    }
}

} // namespace arxglue::sound::nodes