#pragma once

#include "arxsound_types.hpp"
#include <cstddef>

namespace arxsound {

// ============================================================================
// Простая конвертация форматов (без ресемплинга)
// ============================================================================

// Конвертация между форматами с одинаковой частотой дискретизации
AS_result AS_convert_frames(
    void* output,
    uint64_t output_frame_count,
    AS_format output_format,
    uint32_t output_channels,
    const void* input,
    uint64_t input_frame_count,
    AS_format input_format,
    uint32_t input_channels,
    AS_bool32 dither = AS_FALSE  // применить дизеринг при downsample
);

// Получение размера в байтах
constexpr uint32_t AS_bytes_per_frame(AS_format format, uint32_t channels) {
    return AS_bytes_per_sample(format) * channels;
}

// ============================================================================
// Линейный ресемплер (простой, для low-latency сценариев)
// ============================================================================
struct AS_linear_resampler_config {
    AS_format format = AS_format::F32;
    uint32_t channels = 1;
    uint32_t sample_rate_in = 48000;
    uint32_t sample_rate_out = 48000;
    AS_allocation_callbacks allocation_callbacks = AS_allocation_callbacks_init_default();
};

class AS_linear_resampler {
public:
    AS_result init(const AS_linear_resampler_config* config);
    void uninit();
    
    // Ресемплинг
    AS_result process(
        void* output,
        uint64_t* output_frame_count,  // in/out
        const void* input,
        uint64_t input_frame_count
    );
    
    // Сброс состояния (для seek)
    void reset();
    
    // Информация
    uint32_t input_sample_rate() const;
    uint32_t output_sample_rate() const;
    uint32_t channels() const;
    
private:
    struct impl;
    impl* p_impl = nullptr;
};

// ============================================================================
// Конвертер канала (channel remapping)
// ============================================================================
AS_result AS_convert_channels(
    void* output,
    uint32_t output_frame_count,
    uint32_t output_channels,
    const AS_channel* output_map,  // может быть nullptr
    const void* input,
    uint32_t input_frame_count,
    uint32_t input_channels,
    const AS_channel* input_map,   // может быть nullptr
    AS_format format
);

// Инициализация стандартного канал-мапа
void AS_channel_map_init_standard(
    AS_standard_channel_map standard,
    AS_channel* map,
    size_t map_capacity,
    uint32_t channel_count
);

} // namespace arxsound