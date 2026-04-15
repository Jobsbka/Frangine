// arxglue/arxsound_node.cpp
#include "arxsound_node.hpp"
#include "../include/arxsound.hpp"
#include <cstring>
#include <memory>
#include <algorithm>

namespace arxsound::arxglue {

// ============================================================================
// AudioDeviceNode implementation
// ============================================================================

AudioDeviceNode::AudioDeviceNode() {
    m_device = std::make_unique<AS_device>();
}

AudioDeviceNode::~AudioDeviceNode() {
    stop_device();
    if (m_device) {
        m_device->uninit();
    }
}

arxglue::ComponentMetadata AudioDeviceNode::getMetadata() const {
    arxglue::ComponentMetadata metadata;
    metadata.name = "AudioDevice";
    metadata.threadSafe = true;
    metadata.volatile_ = true;
    metadata.pure = false;
    
    // Входы
    metadata.inputs.push_back({"volume", typeid(float), false});
    metadata.inputs.push_back({"start", typeid(bool), false});
    metadata.inputs.push_back({"stop", typeid(bool), false});
    
    // Выходы
    metadata.outputs.push_back({"audio_out", typeid(std::shared_ptr<void>), false});
    metadata.outputs.push_back({"is_running", typeid(bool), false});
    metadata.outputs.push_back({"sample_rate", typeid(uint32_t), false});
    
    return metadata;
}

void AudioDeviceNode::execute(arxglue::Context& ctx) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Получаем параметры из контекста
    float volume = 1.0f;
    bool start_requested = false;
    bool stop_requested = false;
    
    try {
        if (ctx.hasState("volume")) {
            volume = ctx.getState<float>("volume");
        }
        if (ctx.hasState("start")) {
            start_requested = ctx.getState<bool>("start");
        }
        if (ctx.hasState("stop")) {
            stop_requested = ctx.getState<bool>("stop");
        }
    } catch (...) {
        // Игнорируем ошибки каста
    }
    
    // Применяем громкость
    if (m_device && m_device->is_started()) {
        m_device->set_master_volume(std::max(0.0f, std::min(1.0f, volume)));
    }
    
    // Обработка команд старта/стопа
    if (start_requested && !m_running.load()) {
        start_device();
    } else if (stop_requested && m_running.load()) {
        stop_device();
    }
    
    // Устанавливаем выходы
    ctx.output = m_running.load();
    ctx.setState("is_running", m_running.load());
    
    if (m_device) {
        ctx.setState("sample_rate", m_device->sample_rate());
    }
}

void AudioDeviceNode::setParameter(const std::string& name, const std::any& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (name == "type") {
        try {
            m_params.type = std::any_cast<AS_device_type>(value);
        } catch (...) {}
    } else if (name == "format") {
        try {
            m_params.format = std::any_cast<AS_format>(value);
        } catch (...) {}
    } else if (name == "channels") {
        try {
            m_params.channels = std::any_cast<uint32_t>(value);
        } catch (...) {}
    } else if (name == "sample_rate") {
        try {
            m_params.sample_rate = std::any_cast<uint32_t>(value);
        } catch (...) {}
    } else if (name == "volume") {
        try {
            m_params.volume = std::any_cast<float>(value);
        } catch (...) {}
    } else if (name == "auto_start") {
        try {
            m_params.auto_start = std::any_cast<bool>(value);
        } catch (...) {}
    }
}

std::any AudioDeviceNode::getParameter(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (name == "type") return m_params.type;
    if (name == "format") return m_params.format;
    if (name == "channels") return m_params.channels;
    if (name == "sample_rate") return m_params.sample_rate;
    if (name == "volume") return m_params.volume;
    if (name == "auto_start") return m_params.auto_start;
    
    return std::any{};
}

void AudioDeviceNode::serialize(nlohmann::json& j) const {
    j["type"] = "AudioDeviceNode";
    j["params"]["device_type"] = static_cast<int>(m_params.type);
    j["params"]["format"] = static_cast<int>(m_params.format);
    j["params"]["channels"] = m_params.channels;
    j["params"]["sample_rate"] = m_params.sample_rate;
    j["params"]["volume"] = m_params.volume;
    j["params"]["auto_start"] = m_params.auto_start;
}

void AudioDeviceNode::deserialize(const nlohmann::json& j) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (j.contains("params")) {
        const auto& params = j["params"];
        if (params.contains("device_type")) {
            m_params.type = static_cast<AS_device_type>(params["device_type"].get<int>());
        }
        if (params.contains("format")) {
            m_params.format = static_cast<AS_format>(params["format"].get<int>());
        }
        if (params.contains("channels")) {
            m_params.channels = params["channels"].get<uint32_t>();
        }
        if (params.contains("sample_rate")) {
            m_params.sample_rate = params["sample_rate"].get<uint32_t>();
        }
        if (params.contains("volume")) {
            m_params.volume = params["volume"].get<float>();
        }
        if (params.contains("auto_start")) {
            m_params.auto_start = params["auto_start"].get<bool>();
        }
    }
}

AS_result AudioDeviceNode::start_device() {
    if (!m_device) {
        return AS_result::INVALID_OPERATION;
    }
    if (m_running.load()) {
        return AS_result::SUCCESS;  // Уже запущено
    }
    
    // Создаём контекст если нужно
    static std::unique_ptr<AS_context> g_context;
    static std::mutex g_context_mutex;
    
    {
        std::lock_guard<std::mutex> lock(g_context_mutex);
        if (!g_context) {
            g_context = std::make_unique<AS_context>();
            AS_context_config cfg = AS_context_config_init();
            cfg.log = AS_log::default_log();
            g_context->init(nullptr, 0, &cfg);
        }
    }
    
    // Конфигурируем устройство
    AS_device_config config = AS_device_config_init(m_params.type);
    config.playback.format = m_params.format;
    config.playback.channels = m_params.channels;
    config.sample_rate = m_params.sample_rate;
    config.data_callback = data_callback;
    config.user_data = this;
    
    AS_result res = m_device->init(g_context.get(), &config);
    if (!AS_result_is_success(res)) {
        return res;
    }
    
    res = m_device->start();
    if (!AS_result_is_success(res)) {
        m_device->uninit();
        return res;
    }
    
    m_running.store(true);
    return AS_result::SUCCESS;
}

AS_result AudioDeviceNode::stop_device() {
    if (!m_device || !m_running.load()) {
        return AS_result::SUCCESS;
    }
    
    m_device->stop();
    m_device->uninit();
    m_running.store(false);
    
    return AS_result::SUCCESS;
}

void AudioDeviceNode::data_callback(AS_device* device, void* output, const void* input, uint32_t frame_count) {
    (void)device;
    (void)input;
    
    // Заглушка — в реальной реализации здесь будет генерация/обработка аудио
    if (output) {
        // Заполняем тишиной по умолчанию
        AS_device::stream_info info = device->playback_info();
        const size_t bytes_per_sample = AS_bytes_per_sample(info.format);
        const size_t total_bytes = bytes_per_sample * info.channels * frame_count;
        memset(output, 0, total_bytes);
    }
}

// ============================================================================
// AudioBufferNode implementation
// ============================================================================

AudioBufferNode::AudioBufferNode() {
    m_buffer.data.clear();
    m_buffer.format = AS_format::F32;
    m_buffer.channels = 2;
    m_buffer.sample_rate = 48000;
    m_buffer.frame_count = 0;
    m_buffer.cursor = 0;
    m_buffer.looping = false;
}

arxglue::ComponentMetadata AudioBufferNode::getMetadata() const {
    arxglue::ComponentMetadata metadata;
    metadata.name = "AudioBuffer";
    metadata.threadSafe = true;
    metadata.volatile_ = false;
    metadata.pure = true;
    
    // Входы
    metadata.inputs.push_back({"trigger", typeid(bool), false});
    metadata.inputs.push_back({"reset", typeid(bool), false});
    
    // Выходы
    metadata.outputs.push_back({"audio_out", typeid(std::shared_ptr<void>), false});
    metadata.outputs.push_back({"at_end", typeid(bool), false});
    metadata.outputs.push_back({"cursor", typeid(uint64_t), false});
    
    return metadata;
}

void AudioBufferNode::execute(arxglue::Context& ctx) {
    bool trigger = false;
    bool reset = false;
    
    try {
        if (ctx.hasState("trigger")) {
            trigger = ctx.getState<bool>("trigger");
        }
        if (ctx.hasState("reset")) {
            reset = ctx.getState<bool>("reset");
        }
    } catch (...) {}
    
    // Сброс курсора
    if (reset) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer.cursor = 0;
        m_dirty = true;
    }
    
    // Чтение из буфера если триггер
    if (trigger && m_buffer.frame_count > 0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        const uint64_t frames_remaining = m_buffer.frame_count - m_buffer.cursor;
        const uint32_t frames_to_read = static_cast<uint32_t>(std::min<uint64_t>(frames_remaining, 256));
        
        if (frames_to_read > 0) {
            const size_t bytes_per_frame = AS_bytes_per_sample(m_buffer.format) * m_buffer.channels;
            const size_t offset = m_buffer.cursor * bytes_per_frame;
            
            // Создаём копию данных для вывода
            std::vector<uint8_t> chunk(frames_to_read * bytes_per_frame);
            memcpy(chunk.data(), m_buffer.data.data() + offset, chunk.size());
            
            // Упаковываем в shared_ptr для передачи через граф
            auto audio_data = std::make_shared<std::vector<uint8_t>>(std::move(chunk));
            ctx.output = audio_data;
            ctx.setState("audio_out", audio_data);
            
            m_buffer.cursor += frames_to_read;
            m_dirty = true;
        }
        
        // Проверка конца
        const bool at_end = (m_buffer.cursor >= m_buffer.frame_count);
        if (at_end && m_buffer.looping) {
            m_buffer.cursor = 0;  // Зацикливание
        }
        
        ctx.setState("at_end", at_end);
        ctx.setState("cursor", m_buffer.cursor);
    } else {
        ctx.output = std::any{};
        ctx.setState("at_end", m_buffer.cursor >= m_buffer.frame_count);
        ctx.setState("cursor", m_buffer.cursor);
    }
}

void AudioBufferNode::setParameter(const std::string& name, const std::any& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (name == "looping") {
        try {
            m_buffer.looping = std::any_cast<bool>(value);
            m_dirty = true;
        } catch (...) {}
    } else if (name == "format") {
        try {
            m_buffer.format = std::any_cast<AS_format>(value);
            m_dirty = true;
        } catch (...) {}
    } else if (name == "channels") {
        try {
            m_buffer.channels = std::any_cast<uint32_t>(value);
            m_dirty = true;
        } catch (...) {}
    } else if (name == "sample_rate") {
        try {
            m_buffer.sample_rate = std::any_cast<uint32_t>(value);
            m_dirty = true;
        } catch (...) {}
    }
}

std::any AudioBufferNode::getParameter(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (name == "looping") return m_buffer.looping;
    if (name == "format") return m_buffer.format;
    if (name == "channels") return m_buffer.channels;
    if (name == "sample_rate") return m_buffer.sample_rate;
    
    return std::any{};
}

void AudioBufferNode::serialize(nlohmann::json& j) const {
    j["type"] = "AudioBufferNode";
    j["params"]["looping"] = m_buffer.looping;
    j["params"]["format"] = static_cast<int>(m_buffer.format);
    j["params"]["channels"] = m_buffer.channels;
    j["params"]["sample_rate"] = m_buffer.sample_rate;
    j["params"]["frame_count"] = m_buffer.frame_count;
}

void AudioBufferNode::deserialize(const nlohmann::json& j) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (j.contains("params")) {
        const auto& params = j["params"];
        if (params.contains("looping")) {
            m_buffer.looping = params["looping"].get<bool>();
        }
        if (params.contains("format")) {
            m_buffer.format = static_cast<AS_format>(params["format"].get<int>());
        }
        if (params.contains("channels")) {
            m_buffer.channels = params["channels"].get<uint32_t>();
        }
        if (params.contains("sample_rate")) {
            m_buffer.sample_rate = params["sample_rate"].get<uint32_t>();
        }
        if (params.contains("frame_count")) {
            m_buffer.frame_count = params["frame_count"].get<uint64_t>();
        }
    }
    m_dirty = true;
}

void AudioBufferNode::set_buffer(
    const void* data,
    uint64_t frame_count,
    AS_format format,
    uint32_t channels,
    uint32_t sample_rate
) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!data || frame_count == 0 || channels == 0) {
        return;
    }
    
    const size_t bytes_per_frame = AS_bytes_per_sample(format) * channels;
    const size_t total_bytes = frame_count * bytes_per_frame;
    
    m_buffer.data.resize(total_bytes);
    memcpy(m_buffer.data.data(), data, total_bytes);
    
    m_buffer.format = format;
    m_buffer.channels = channels;
    m_buffer.sample_rate = sample_rate;
    m_buffer.frame_count = frame_count;
    m_buffer.cursor = 0;
    m_dirty = true;
}

// ============================================================================
// Node registration
// ============================================================================

void register_audio_nodes() {
    // В реальной реализации здесь будет регистрация в NodeFactory
    // Пример:
    // auto& factory = arxglue::NodeFactory::instance();
    // factory.registerNode("AudioDeviceNode", []() {
    //     return std::make_unique<AudioDeviceNode>();
    // });
    // factory.registerNode("AudioBufferNode", []() {
    //     return std::make_unique<AudioBufferNode>();
    // });
}

} // namespace arxsound::arxglue