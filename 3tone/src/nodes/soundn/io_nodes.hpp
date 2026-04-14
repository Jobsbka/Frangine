#pragma once
#include "../../sound/audio_node.hpp"
#include "../../sound/audio_device.hpp"
#include "../../sound/audio_asset.hpp"
#include <memory>
#include <string>

namespace arxglue::sound::nodes {

class AudioOutputNode : public AudioNode {
public:
    AudioOutputNode();
    explicit AudioOutputNode(std::shared_ptr<AudioDevice> device);
    ~AudioOutputNode() override;

    void prepare(const AudioSpec& spec) override;
    void process(AudioBuffer& output) override;
    void reset() override;

    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

    // Callback для AudioDevice
    void audioCallback(float** out, float** /*in*/, int numFrames, int numChannels);

private:
    std::shared_ptr<AudioDevice> m_device;
    bool m_deviceStarted = false;
    AudioBuffer m_outputBuffer;
    std::mutex m_bufferMutex;
    std::atomic<bool> m_hasNewData{false};
};

class AudioFileWriterNode : public AudioNode {
public:
    AudioFileWriterNode();
    explicit AudioFileWriterNode(const std::string& path);

    void prepare(const AudioSpec& spec) override;
    void process(AudioBuffer& output) override;
    void reset() override;

    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

private:
    std::string m_path;
    std::unique_ptr<AudioBuffer> m_accumBuffer;
    size_t m_framesWritten = 0;
};

class AudioFileReaderNode : public AudioNode {
public:
    AudioFileReaderNode();
    explicit AudioFileReaderNode(const std::string& path);

    void prepare(const AudioSpec& spec) override;
    void process(AudioBuffer& output) override;
    void reset() override;

    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

private:
    std::string m_path;
    std::shared_ptr<AudioAsset> m_asset;
    size_t m_playhead = 0;
    bool m_loop = false;
};

} // namespace arxglue::sound::nodes