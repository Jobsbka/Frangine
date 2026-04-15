#pragma once

#include "../include/arxsound.hpp"
#include "../src/core/node.hpp"
#include <memory>

namespace arxsound::arxglue {

// ============================================================================
// AudioDeviceNode - узел для работы с аудио-устройством в графе ArxGlue
// ============================================================================
class AudioDeviceNode : public arxglue::INode {
public:
    AudioDeviceNode();
    ~AudioDeviceNode() override;
    
    // INode interface
    void execute(arxglue::Context& ctx) override;
    arxglue::ComponentMetadata getMetadata() const override;
    
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;
    
    // Audio-specific
    AS_result start_device();
    AS_result stop_device();
    
private:
    // Внутреннее устройство
    std::unique_ptr<AS_device> m_device;
    
    // Параметры (кешируются)
    struct {
        arxsound::AS_device_type type = arxsound::AS_device_type::PLAYBACK;
        arxsound::AS_format format = arxsound::AS_format::F32;
        uint32_t channels = 2;
        uint32_t sample_rate = 48000;
        float volume = 1.0f;
        bool auto_start = true;
    } m_params;
    
    // Callback handler
    static void data_callback(AS_device* device, void* output, const void* input, uint32_t frame_count);
    
    // Состояние
    std::atomic<bool> m_running{false};
    mutable std::mutex m_mutex;
};

// ============================================================================
// AudioBufferNode - узел для работы с буфером аудио-данных
// ============================================================================
class AudioBufferNode : public arxglue::INode {
public:
    AudioBufferNode();
    
    void execute(arxglue::Context& ctx) override;
    arxglue::ComponentMetadata getMetadata() const override;
    
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;
    
    // Установка буфера
    void set_buffer(
        const void* data,
        uint64_t frame_count,
        arxsound::AS_format format,
        uint32_t channels,
        uint32_t sample_rate
    );
    
private:
    struct Buffer {
        std::vector<uint8_t> data;
        arxsound::AS_format format = arxsound::AS_format::F32;
        uint32_t channels = 2;
        uint32_t sample_rate = 48000;
        uint64_t frame_count = 0;
        uint64_t cursor = 0;
        bool looping = false;
    };
    
    Buffer m_buffer;
    bool m_dirty = true;
};

// ============================================================================
// Регистрация узлов в NodeFactory
// ============================================================================
void register_audio_nodes();

} // namespace arxsound::arxglue