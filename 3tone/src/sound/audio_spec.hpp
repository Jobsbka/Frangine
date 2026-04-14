#pragma once
#include <cstdint>

namespace arxglue::sound {

struct AudioSpec {
    uint32_t sampleRate = 44100;
    uint16_t numChannels = 2;
    uint16_t blockSize = 512;   // размер блока для потоковой обработки

    bool operator==(const AudioSpec& other) const {
        return sampleRate == other.sampleRate &&
               numChannels == other.numChannels;
    }

    bool operator!=(const AudioSpec& other) const {
        return !(*this == other);
    }
};

} // namespace arxglue::sound