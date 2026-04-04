#include "JsonSerializer.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace frabgine {

JsonSerializer::JsonSerializer(Mode mode) : mode_(mode) {
    if (mode == Mode::Writing) {
        root_ = nlohmann::json::object();
        current_ = &root_;
    }
}

bool JsonSerializer::loadFromFile(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) return false;
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        root_ = nlohmann::json::parse(buffer.str());
        current_ = &root_;
        mode_ = Mode::Reading;
        return true;
    } catch (...) {
        return false;
    }
}

bool JsonSerializer::saveToFile(const std::string& filename) const {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) return false;
        
        file << root_.dump(4); // 4 пробела для форматирования
        return true;
    } catch (...) {
        return false;
    }
}

void JsonSerializer::beginObject(const std::string& name) {
    if (isWriting()) {
        (*current_)[name] = nlohmann::json::object();
        stack_.push_back(current_);
        current_ = &(*current_)[name];
    } else {
        if (current_->contains(name)) {
            stack_.push_back(current_);
            current_ = &(*current_)[name];
        } else {
            ok_ = false;
        }
    }
}

void JsonSerializer::endObject() {
    if (stack_.empty()) return;
    
    current_ = stack_.back();
    stack_.pop_back();
}

void JsonSerializer::serialize(int8_t& value) {
    int32_t tmp = value;
    serialize(tmp);
    value = static_cast<int8_t>(tmp);
}

void JsonSerializer::serialize(uint8_t& value) {
    uint32_t tmp = value;
    serialize(tmp);
    value = static_cast<uint8_t>(tmp);
}

void JsonSerializer::serialize(int16_t& value) {
    int32_t tmp = value;
    serialize(tmp);
    value = static_cast<int16_t>(tmp);
}

void JsonSerializer::serialize(uint16_t& value) {
    uint32_t tmp = value;
    serialize(tmp);
    value = static_cast<uint16_t>(tmp);
}

void JsonSerializer::serialize(int32_t& value) {
    if (isWriting()) {
        *current_ = value;
    } else {
        if (current_->is_number_integer()) {
            value = current_->get<int32_t>();
        } else {
            ok_ = false;
        }
    }
}

void JsonSerializer::serialize(uint32_t& value) {
    if (isWriting()) {
        *current_ = value;
    } else {
        if (current_->is_number_unsigned()) {
            value = current_->get<uint32_t>();
        } else {
            ok_ = false;
        }
    }
}

void JsonSerializer::serialize(int64_t& value) {
    if (isWriting()) {
        *current_ = value;
    } else {
        if (current_->is_number_integer()) {
            value = current_->get<int64_t>();
        } else {
            ok_ = false;
        }
    }
}

void JsonSerializer::serialize(uint64_t& value) {
    if (isWriting()) {
        *current_ = value;
    } else {
        if (current_->is_number_unsigned()) {
            value = current_->get<uint64_t>();
        } else {
            ok_ = false;
        }
    }
}

void JsonSerializer::serialize(float& value) {
    if (isWriting()) {
        *current_ = value;
    } else {
        if (current_->is_number_float()) {
            value = current_->get<float>();
        } else {
            ok_ = false;
        }
    }
}

void JsonSerializer::serialize(double& value) {
    if (isWriting()) {
        *current_ = value;
    } else {
        if (current_->is_number_float()) {
            value = current_->get<double>();
        } else {
            ok_ = false;
        }
    }
}

void JsonSerializer::serialize(bool& value) {
    if (isWriting()) {
        *current_ = value;
    } else {
        if (current_->is_boolean()) {
            value = current_->get<bool>();
        } else {
            ok_ = false;
        }
    }
}

void JsonSerializer::serialize(std::string& value) {
    if (isWriting()) {
        *current_ = value;
    } else {
        if (current_->is_string()) {
            value = current_->get<std::string>();
        } else {
            ok_ = false;
        }
    }
}

} // namespace frabgine
