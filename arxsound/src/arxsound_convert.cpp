// src/arxsound_convert.cpp
#include "../include/arxsound_convert.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>
#include <memory>

namespace arxsound {

// ============================================================================
// Вспомогательные функции конвертации
// ============================================================================

// Конвертация U8 -> F32
static void convert_u8_to_f32(float* out, const uint8_t* in, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        out[i] = (static_cast<float>(in[i]) - 128.0f) / 128.0f;
    }
}

// Конвертация F32 -> U8
static void convert_f32_to_u8(uint8_t* out, const float* in, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        float clamped = std::max(-1.0f, std::min(1.0f, in[i]));
        out[i] = static_cast<uint8_t>((clamped * 128.0f) + 128.0f);
    }
}

// Конвертация S16 -> F32
static void convert_s16_to_f32(float* out, const int16_t* in, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        out[i] = static_cast<float>(in[i]) / 32768.0f;
    }
}

// Конвертация F32 -> S16
static void convert_f32_to_s16(int16_t* out, const float* in, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        float clamped = std::max(-1.0f, std::min(1.0f, in[i]));
        out[i] = static_cast<int16_t>(clamped * 32767.0f);
    }
}

// Конвертация S24 (packed) -> F32
static void convert_s24_to_f32(float* out, const uint8_t* in, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* byte = in + (i * 3);
        int32_t sample = static_cast<int32_t>(byte[0]) |
                        (static_cast<int32_t>(byte[1]) << 8) |
                        (static_cast<int32_t>(byte[2]) << 16);
        // Sign extend from 24-bit
        if (sample & 0x800000) {
            sample |= 0xFF000000;
        }
        out[i] = static_cast<float>(sample) / 8388608.0f;
    }
}

// Конвертация F32 -> S24 (packed)
static void convert_f32_to_s24(uint8_t* out, const float* in, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        float clamped = std::max(-1.0f, std::min(1.0f, in[i]));
        int32_t sample = static_cast<int32_t>(clamped * 8388607.0f);
        uint8_t* byte = out + (i * 3);
        byte[0] = static_cast<uint8_t>(sample & 0xFF);
        byte[1] = static_cast<uint8_t>((sample >> 8) & 0xFF);
        byte[2] = static_cast<uint8_t>((sample >> 16) & 0xFF);
    }
}

// Конвертация S32 -> F32
static void convert_s32_to_f32(float* out, const int32_t* in, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        out[i] = static_cast<float>(in[i]) / 2147483648.0f;
    }
}

// Конвертация F32 -> S32
static void convert_f32_to_s32(int32_t* out, const float* in, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        float clamped = std::max(-1.0f, std::min(1.0f, in[i]));
        out[i] = static_cast<int32_t>(clamped * 2147483647.0f);
    }
}

// ============================================================================
// Конвертация между произвольными форматами
// ============================================================================

static AS_result convert_frames_internal(
    void* output,
    uint64_t output_frame_count,
    AS_format output_format,
    uint32_t output_channels,
    const void* input,
    uint64_t input_frame_count,
    AS_format input_format,
    uint32_t input_channels,
    AS_bool32 dither
) {
    if (!output || !input) {
        return AS_result::INVALID_ARGS;
    }
    if (output_frame_count == 0 || input_frame_count == 0) {
        return AS_result::SUCCESS;
    }
    if (output_channels == 0 || input_channels == 0) {
        return AS_result::INVALID_ARGS;
    }
    
    const uint64_t frames_to_process = std::min(output_frame_count, input_frame_count);
    const uint32_t bytes_per_sample_in = AS_bytes_per_sample(input_format);
    const uint32_t bytes_per_sample_out = AS_bytes_per_sample(output_format);
    
    if (bytes_per_sample_in == 0 || bytes_per_sample_out == 0) {
        return AS_result::FORMAT_NOT_SUPPORTED;
    }
    
    // Если форматы одинаковые и каналы совпадают — просто копируем
    if (input_format == output_format && input_channels == output_channels) {
        const size_t bytes_to_copy = frames_to_process * bytes_per_sample_in * input_channels;
        memcpy(output, input, bytes_to_copy);
        return AS_result::SUCCESS;
    }
    
    // Для сложных конвертаций используем F32 как промежуточный формат
    std::vector<float> temp_buffer;
    float* temp_in = nullptr;
    float* temp_out = nullptr;
    
    // Если вход не F32 — конвертируем в F32
    if (input_format != AS_format::F32) {
        temp_buffer.resize(frames_to_process * input_channels);
        temp_in = temp_buffer.data();
        
        switch (input_format) {
            case AS_format::U8:
                convert_u8_to_f32(temp_in, static_cast<const uint8_t*>(input), 
                                 frames_to_process * input_channels);
                break;
            case AS_format::S16:
                convert_s16_to_f32(temp_in, static_cast<const int16_t*>(input),
                                  frames_to_process * input_channels);
                break;
            case AS_format::S24:
                convert_s24_to_f32(temp_in, static_cast<const uint8_t*>(input),
                                  frames_to_process * input_channels);
                break;
            case AS_format::S32:
                convert_s32_to_f32(temp_in, static_cast<const int32_t*>(input),
                                  frames_to_process * input_channels);
                break;
            default:
                return AS_result::FORMAT_NOT_SUPPORTED;
        }
    } else {
        temp_in = const_cast<float*>(static_cast<const float*>(input));
    }
    
    // Если выход не F32 — конвертируем из F32
    float* temp_final = temp_in;
    if (output_format != AS_format::F32) {
        if (input_format != AS_format::F32) {
            // Нужен второй временный буфер для channel conversion
            temp_buffer.resize(frames_to_process * output_channels);
            temp_final = temp_buffer.data();
        }
        
        // Конвертация каналов если нужно
        if (input_channels != output_channels) {
            // Простая стратегия: mono -> stereo (duplicate), stereo -> mono (average)
            if (input_channels == 1 && output_channels == 2) {
                for (uint64_t f = 0; f < frames_to_process; ++f) {
                    temp_final[f * 2] = temp_in[f];
                    temp_final[f * 2 + 1] = temp_in[f];
                }
            } else if (input_channels == 2 && output_channels == 1) {
                for (uint64_t f = 0; f < frames_to_process; ++f) {
                    temp_final[f] = (temp_in[f * 2] + temp_in[f * 2 + 1]) * 0.5f;
                }
            } else {
                // Для других комбинаций — заполняем первые каналы, остальные тишиной
                for (uint64_t f = 0; f < frames_to_process; ++f) {
                    for (uint32_t ch = 0; ch < output_channels; ++ch) {
                        if (ch < input_channels) {
                            temp_final[f * output_channels + ch] = temp_in[f * input_channels + ch];
                        } else {
                            temp_final[f * output_channels + ch] = 0.0f;
                        }
                    }
                }
            }
        } else {
            temp_final = temp_in;
        }
        
        // Конвертация формата
        switch (output_format) {
            case AS_format::U8:
                convert_f32_to_u8(static_cast<uint8_t*>(output), temp_final,
                                 frames_to_process * output_channels);
                break;
            case AS_format::S16:
                convert_f32_to_s16(static_cast<int16_t*>(output), temp_final,
                                  frames_to_process * output_channels);
                break;
            case AS_format::S24:
                convert_f32_to_s24(static_cast<uint8_t*>(output), temp_final,
                                  frames_to_process * output_channels);
                break;
            case AS_format::S32:
                convert_f32_to_s32(static_cast<int32_t*>(output), temp_final,
                                  frames_to_process * output_channels);
                break;
            default:
                return AS_result::FORMAT_NOT_SUPPORTED;
        }
    } else {
        // Выход F32
        if (input_channels != output_channels) {
            // Конвертация каналов в выходной буфер
            float* out_f32 = static_cast<float*>(output);
            if (input_channels == 1 && output_channels == 2) {
                for (uint64_t f = 0; f < frames_to_process; ++f) {
                    out_f32[f * 2] = temp_in[f];
                    out_f32[f * 2 + 1] = temp_in[f];
                }
            } else if (input_channels == 2 && output_channels == 1) {
                for (uint64_t f = 0; f < frames_to_process; ++f) {
                    out_f32[f] = (temp_in[f * 2] + temp_in[f * 2 + 1]) * 0.5f;
                }
            } else {
                for (uint64_t f = 0; f < frames_to_process; ++f) {
                    for (uint32_t ch = 0; ch < output_channels; ++ch) {
                        if (ch < input_channels) {
                            out_f32[f * output_channels + ch] = temp_in[f * input_channels + ch];
                        } else {
                            out_f32[f * output_channels + ch] = 0.0f;
                        }
                    }
                }
            }
        } else if (input_format != AS_format::F32) {
            // Копируем из temp_in в выход
            memcpy(output, temp_in, frames_to_process * output_channels * sizeof(float));
        }
        // Иначе данные уже в выходном буфере (input == output и форматы совпадают)
    }
    
    return AS_result::SUCCESS;
}

AS_API AS_result AS_convert_frames(
    void* output,
    uint64_t output_frame_count,
    AS_format output_format,
    uint32_t output_channels,
    const void* input,
    uint64_t input_frame_count,
    AS_format input_format,
    uint32_t input_channels,
    AS_bool32 dither
) {
    return convert_frames_internal(
        output, output_frame_count, output_format, output_channels,
        input, input_frame_count, input_format, input_channels,
        dither
    );
}

// ============================================================================
// AS_linear_resampler implementation
// ============================================================================

struct AS_linear_resampler::impl {
    AS_format format{AS_format::F32};
    uint32_t channels{1};
    uint32_t sample_rate_in{48000};
    uint32_t sample_rate_out{48000};
    AS_allocation_callbacks allocation_callbacks;
    
    // Состояние ресемплера
    double fractional_index{0.0};
    std::vector<float> previous_frame;
    bool initialized{false};
    
    impl() {
        allocation_callbacks = AS_allocation_callbacks_init_default();
    }
};

AS_linear_resampler::AS_linear_resampler() = default;
AS_linear_resampler::~AS_linear_resampler() {
    uninit();
}

AS_result AS_linear_resampler::init(const AS_linear_resampler_config* config) {
    if (!config) {
        return AS_result::INVALID_ARGS;
    }
    if (config->sample_rate_in == 0 || config->sample_rate_out == 0) {
        return AS_result::INVALID_ARGS;
    }
    if (config->channels == 0 || config->channels > 256) {
        return AS_result::INVALID_ARGS;
    }
    
    if (!p_impl) {
        p_impl = new(std::nothrow) impl();
        if (!p_impl) {
            return AS_result::OUT_OF_MEMORY;
        }
    }
    
    p_impl->format = config->format;
    p_impl->channels = config->channels;
    p_impl->sample_rate_in = config->sample_rate_in;
    p_impl->sample_rate_out = config->sample_rate_out;
    p_impl->allocation_callbacks = config->allocation_callbacks;
    
    // Инициализируем предыдущий фрейм нулями
    p_impl->previous_frame.resize(config->channels, 0.0f);
    p_impl->fractional_index = 0.0;
    p_impl->initialized = true;
    
    return AS_result::SUCCESS;
}

void AS_linear_resampler::uninit() {
    if (p_impl) {
        delete p_impl;
        p_impl = nullptr;
    }
}

AS_result AS_linear_resampler::process(
    void* output,
    uint64_t* output_frame_count,
    const void* input,
    uint64_t input_frame_count
) {
    if (!p_impl || !p_impl->initialized) {
        return AS_result::INVALID_OPERATION;
    }
    if (!output || !input || !output_frame_count) {
        return AS_result::INVALID_ARGS;
    }
    if (p_impl->format != AS_format::F32) {
        return AS_result::FORMAT_NOT_SUPPORTED;  // Линейный ресемплер работает только с F32
    }
    
    const uint32_t channels = p_impl->channels;
    const uint32_t sr_in = p_impl->sample_rate_in;
    const uint32_t sr_out = p_impl->sample_rate_out;
    
    if (sr_in == sr_out) {
        // Без ресемплинга — просто копируем
        const uint64_t frames_to_copy = std::min(*output_frame_count, input_frame_count);
        const size_t bytes_to_copy = frames_to_copy * channels * sizeof(float);
        memcpy(output, input, bytes_to_copy);
        *output_frame_count = frames_to_copy;
        return AS_result::SUCCESS;
    }
    
    float* out_f32 = static_cast<float*>(output);
    const float* in_f32 = static_cast<const float*>(input);
    
    const double ratio = static_cast<double>(sr_in) / static_cast<double>(sr_out);
    
    uint64_t out_frames_written = 0;
    const uint64_t max_out_frames = *output_frame_count;
    
    // Линейная интерполяция
    for (uint64_t out_frame = 0; out_frame < max_out_frames; ++out_frame) {
        // Индекс во входном буфере
        const uint64_t in_index = static_cast<uint64_t>(p_impl->fractional_index);
        
        // Если вышли за пределы входного буфера — останавливаемся
        if (in_index >= input_frame_count) {
            break;
        }
        
        // Фракционная часть для интерполяции
        const double fraction = p_impl->fractional_index - static_cast<double>(in_index);
        
        // Получаем два соседних сэмпла для каждого канала
        const float* sample0 = in_f32 + (in_index * channels);
        const float* sample1 = (in_index + 1 < input_frame_count) 
            ? in_f32 + ((in_index + 1) * channels)
            : p_impl->previous_frame.data();
        
        // Линейная интерполяция для каждого канала
        for (uint32_t ch = 0; ch < channels; ++ch) {
            const float s0 = sample0[ch];
            const float s1 = sample1[ch];
            out_f32[out_frame * channels + ch] = static_cast<float>(s0 + (s1 - s0) * fraction);
        }
        
        // Сохраняем последний фрейм для следующего вызова
        for (uint32_t ch = 0; ch < channels; ++ch) {
            p_impl->previous_frame[ch] = sample0[ch];
        }
        
        // Продвигаем индекс
        p_impl->fractional_index += ratio;
        ++out_frames_written;
    }
    
    *output_frame_count = out_frames_written;
    return AS_result::SUCCESS;
}

void AS_linear_resampler::reset() {
    if (p_impl) {
        p_impl->fractional_index = 0.0;
        std::fill(p_impl->previous_frame.begin(), p_impl->previous_frame.end(), 0.0f);
    }
}

uint32_t AS_linear_resampler::input_sample_rate() const {
    return p_impl ? p_impl->sample_rate_in : 0;
}

uint32_t AS_linear_resampler::output_sample_rate() const {
    return p_impl ? p_impl->sample_rate_out : 0;
}

uint32_t AS_linear_resampler::channels() const {
    return p_impl ? p_impl->channels : 0;
}

// ============================================================================
// Channel remapping
// ============================================================================

AS_API AS_result AS_convert_channels(
    void* output,
    uint32_t output_frame_count,
    uint32_t output_channels,
    const AS_channel* output_map,
    const void* input,
    uint32_t input_frame_count,
    uint32_t input_channels,
    const AS_channel* input_map,
    AS_format format
) {
    if (!output || !input) {
        return AS_result::INVALID_ARGS;
    }
    if (output_frame_count == 0 || input_frame_count == 0) {
        return AS_result::SUCCESS;
    }
    if (output_channels == 0 || input_channels == 0) {
        return AS_result::INVALID_ARGS;
    }
    if (format == AS_format::UNKNOWN) {
        return AS_result::INVALID_ARGS;
    }
    
    const uint32_t bytes_per_sample = AS_bytes_per_sample(format);
    if (bytes_per_sample == 0) {
        return AS_result::FORMAT_NOT_SUPPORTED;
    }
    
    const uint32_t frames_to_process = std::min(output_frame_count, input_frame_count);
    const uint8_t* in_bytes = static_cast<const uint8_t*>(input);
    uint8_t* out_bytes = static_cast<uint8_t*>(output);
    
    // Если карты каналов не указаны — используем identity mapping
    std::vector<AS_channel> default_in_map(input_channels);
    std::vector<AS_channel> default_out_map(output_channels);
    
    if (!input_map) {
        for (uint32_t i = 0; i < input_channels; ++i) {
            default_in_map[i] = static_cast<AS_channel>(i);
        }
        input_map = default_in_map.data();
    }
    if (!output_map) {
        for (uint32_t i = 0; i < output_channels; ++i) {
            default_out_map[i] = static_cast<AS_channel>(i);
        }
        output_map = default_out_map.data();
    }
    
    // Создаём таблицу перемаппинга
    std::vector<int32_t> channel_map(output_channels, -1);
    for (uint32_t out_ch = 0; out_ch < output_channels; ++out_ch) {
        for (uint32_t in_ch = 0; in_ch < input_channels; ++in_ch) {
            if (output_map[out_ch] == input_map[in_ch]) {
                channel_map[out_ch] = static_cast<int32_t>(in_ch);
                break;
            }
        }
    }
    
    // Копируем/перемешиваем каналы
    for (uint32_t frame = 0; frame < frames_to_process; ++frame) {
        for (uint32_t out_ch = 0; out_ch < output_channels; ++out_ch) {
            const int32_t in_ch = channel_map[out_ch];
            const size_t out_offset = (frame * output_channels + out_ch) * bytes_per_sample;
            
            if (in_ch >= 0 && static_cast<uint32_t>(in_ch) < input_channels) {
                const size_t in_offset = (frame * input_channels + static_cast<uint32_t>(in_ch)) * bytes_per_sample;
                memcpy(out_bytes + out_offset, in_bytes + in_offset, bytes_per_sample);
            } else {
                // Заполняем тишиной если канал не найден
                memset(out_bytes + out_offset, 0, bytes_per_sample);
            }
        }
    }
    
    return AS_result::SUCCESS;
}

void AS_channel_map_init_standard(
    AS_standard_channel_map standard,
    AS_channel* map,
    size_t map_capacity,
    uint32_t channel_count
) {
    if (!map || map_capacity < channel_count) {
        return;
    }
    
    // Стандартные мапы каналов (упрощённая версия)
    // В полной реализации нужно использовать полные определения из miniaudio
    switch (standard) {
        case AS_standard_channel_map::DEFAULT:
        case AS_standard_channel_map::WAV:
            // Mono, Stereo, 2.1, 5.1, 7.1
            {
                static const AS_channel wav_map[] = {
                    0,                                          // 1.0
                    0, 1,                                       // 2.0
                    0, 1, 2,                                    // 2.1
                    0, 1, 2, 3, 4, 5,                          // 5.1
                    0, 1, 2, 3, 4, 5, 6, 7                     // 7.1
                };
                const size_t offset = (channel_count >= 1 && channel_count <= 8) 
                    ? (channel_count == 3 ? 2 : channel_count - 1) 
                    : 0;
                const size_t start = (channel_count == 3) ? 2 : 
                                    (channel_count > 8 ? 6 : channel_count - 1);
                for (uint32_t i = 0; i < channel_count && i < map_capacity; ++i) {
                    map[i] = i;
                }
            }
            break;
            
        case AS_standard_channel_map::WEBCOMP:
            // WebRTC-style (L, R для стерео)
            for (uint32_t i = 0; i < channel_count && i < map_capacity; ++i) {
                map[i] = i;
            }
            break;
            
        case AS_standard_channel_map::ALSA:
        case AS_standard_channel_map::PULSEAUDIO:
        case AS_standard_channel_map::COREAUDIO:
            // Платформенно-специфичные мапы
            // В полной реализации нужно использовать определения из miniaudio
            for (uint32_t i = 0; i < channel_count && i < map_capacity; ++i) {
                map[i] = i;
            }
            break;
            
        default:
            for (uint32_t i = 0; i < channel_count && i < map_capacity; ++i) {
                map[i] = i;
            }
            break;
    }
}

} // namespace arxsound