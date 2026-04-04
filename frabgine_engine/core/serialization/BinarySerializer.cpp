#include "BinarySerializer.hpp"
#include <fstream>
#include <cstring>

namespace frabgine {

BinarySerializer::BinarySerializer(Mode mode) : mode_(mode) {
    if (mode == Mode::Writing) {
        // Запись magic number и версии
        uint32_t magic = 0x46524142; // "FRAB"
        uint16_t version = 1;
        buffer_.insert(buffer_.end(), 
            reinterpret_cast<uint8_t*>(&magic), 
            reinterpret_cast<uint8_t*>(&magic) + sizeof(magic));
        buffer_.insert(buffer_.end(), 
            reinterpret_cast<uint8_t*>(&version), 
            reinterpret_cast<uint8_t*>(&version) + sizeof(version));
    }
}

bool BinarySerializer::loadFromFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    buffer_.resize(size);
    if (!file.read(reinterpret_cast<char*>(buffer_.data()), size)) {
        return false;
    }
    
    // Проверка magic number
    uint32_t magic;
    std::memcpy(&magic, buffer_.data(), sizeof(magic));
    if (magic != 0x46524142) return false;
    
    position_ = sizeof(magic) + sizeof(uint16_t); // Пропуск magic и version
    mode_ = Mode::Reading;
    return true;
}

bool BinarySerializer::saveToFile(const std::string& filename) const {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;
    
    file.write(reinterpret_cast<const char*>(buffer_.data()), buffer_.size());
    return file.good();
}

void BinarySerializer::ensureSize(size_t bytes) {
    if (position_ + bytes > buffer_.size()) {
        ok_ = false;
    }
}

void BinarySerializer::serialize(int8_t& value) {
    if (isWriting()) {
        buffer_.insert(buffer_.end(), 
            reinterpret_cast<uint8_t*>(&value), 
            reinterpret_cast<uint8_t*>(&value) + sizeof(value));
    } else {
        ensureSize(sizeof(value));
        if (isOk()) {
            std::memcpy(&value, buffer_.data() + position_, sizeof(value));
            position_ += sizeof(value);
        }
    }
}

void BinarySerializer::serialize(uint8_t& value) {
    if (isWriting()) {
        buffer_.insert(buffer_.end(), value);
    } else {
        ensureSize(sizeof(value));
        if (isOk()) {
            value = buffer_[position_++];
        }
    }
}

void BinarySerializer::serialize(int16_t& value) {
    if (isWriting()) {
        uint16_t tmp = static_cast<uint16_t>(value);
        buffer_.push_back(tmp & 0xFF);
        buffer_.push_back((tmp >> 8) & 0xFF);
    } else {
        ensureSize(2);
        if (isOk()) {
            uint16_t tmp = buffer_[position_] | (buffer_[position_ + 1] << 8);
            value = static_cast<int16_t>(tmp);
            position_ += 2;
        }
    }
}

void BinarySerializer::serialize(uint16_t& value) {
    if (isWriting()) {
        buffer_.push_back(value & 0xFF);
        buffer_.push_back((value >> 8) & 0xFF);
    } else {
        ensureSize(2);
        if (isOk()) {
            value = buffer_[position_] | (buffer_[position_ + 1] << 8);
            position_ += 2;
        }
    }
}

void BinarySerializer::serialize(int32_t& value) {
    if (isWriting()) {
        uint32_t tmp = static_cast<uint32_t>(value);
        for (int i = 0; i < 4; ++i) {
            buffer_.push_back((tmp >> (i * 8)) & 0xFF);
        }
    } else {
        ensureSize(4);
        if (isOk()) {
            uint32_t tmp = 0;
            for (int i = 0; i < 4; ++i) {
                tmp |= static_cast<uint32_t>(buffer_[position_ + i]) << (i * 8);
            }
            value = static_cast<int32_t>(tmp);
            position_ += 4;
        }
    }
}

void BinarySerializer::serialize(uint32_t& value) {
    if (isWriting()) {
        for (int i = 0; i < 4; ++i) {
            buffer_.push_back((value >> (i * 8)) & 0xFF);
        }
    } else {
        ensureSize(4);
        if (isOk()) {
            value = 0;
            for (int i = 0; i < 4; ++i) {
                value |= static_cast<uint32_t>(buffer_[position_ + i]) << (i * 8);
            }
            position_ += 4;
        }
    }
}

void BinarySerializer::serialize(int64_t& value) {
    if (isWriting()) {
        uint64_t tmp = static_cast<uint64_t>(value);
        for (int i = 0; i < 8; ++i) {
            buffer_.push_back((tmp >> (i * 8)) & 0xFF);
        }
    } else {
        ensureSize(8);
        if (isOk()) {
            uint64_t tmp = 0;
            for (int i = 0; i < 8; ++i) {
                tmp |= static_cast<uint64_t>(buffer_[position_ + i]) << (i * 8);
            }
            value = static_cast<int64_t>(tmp);
            position_ += 8;
        }
    }
}

void BinarySerializer::serialize(uint64_t& value) {
    if (isWriting()) {
        for (int i = 0; i < 8; ++i) {
            buffer_.push_back((value >> (i * 8)) & 0xFF);
        }
    } else {
        ensureSize(8);
        if (isOk()) {
            value = 0;
            for (int i = 0; i < 8; ++i) {
                value |= static_cast<uint64_t>(buffer_[position_ + i]) << (i * 8);
            }
            position_ += 8;
        }
    }
}

void BinarySerializer::serialize(float& value) {
    if (isWriting()) {
        uint32_t tmp;
        std::memcpy(&tmp, &value, sizeof(float));
        serialize(tmp);
    } else {
        uint32_t tmp;
        serialize(tmp);
        if (isOk()) {
            std::memcpy(&value, &tmp, sizeof(float));
        }
    }
}

void BinarySerializer::serialize(double& value) {
    if (isWriting()) {
        uint64_t tmp;
        std::memcpy(&tmp, &value, sizeof(double));
        serialize(tmp);
    } else {
        uint64_t tmp;
        serialize(tmp);
        if (isOk()) {
            std::memcpy(&value, &tmp, sizeof(double));
        }
    }
}

void BinarySerializer::serialize(bool& value) {
    uint8_t tmp = value ? 1 : 0;
    serialize(tmp);
    value = (tmp != 0);
}

void BinarySerializer::serialize(std::string& value) {
    if (isWriting()) {
        uint32_t length = static_cast<uint32_t>(value.size());
        serialize(length);
        buffer_.insert(buffer_.end(), 
            reinterpret_cast<const uint8_t*>(value.data()), 
            reinterpret_cast<const uint8_t*>(value.data()) + length);
    } else {
        uint32_t length;
        serialize(length);
        if (isOk()) {
            ensureSize(length);
            if (isOk()) {
                value.assign(
                    reinterpret_cast<const char*>(buffer_.data() + position_), 
                    length);
                position_ += length;
            }
        }
    }
}

} // namespace frabgine
