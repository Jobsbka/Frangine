#ifndef FRABGINE_CORE_SERIALIZATION_SERIALIZER_HPP
#define FRABGINE_CORE_SERIALIZATION_SERIALIZER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace frabgine {

// Базовый класс для всех сериализаторов
class Serializer {
public:
    enum class Mode {
        Reading,
        Writing
    };
    
protected:
    Mode mode_;
    bool ok_ = true;
    
public:
    virtual ~Serializer() = default;
    
    Mode mode() const { return mode_; }
    bool isReading() const { return mode_ == Mode::Reading; }
    bool isWriting() const { return mode_ == Mode::Writing; }
    bool isOk() const { return ok_; }
    void fail() { ok_ = false; }
    
    // Базовые типы
    virtual void serialize(int8_t& value) = 0;
    virtual void serialize(uint8_t& value) = 0;
    virtual void serialize(int16_t& value) = 0;
    virtual void serialize(uint16_t& value) = 0;
    virtual void serialize(int32_t& value) = 0;
    virtual void serialize(uint32_t& value) = 0;
    virtual void serialize(int64_t& value) = 0;
    virtual void serialize(uint64_t& value) = 0;
    virtual void serialize(float& value) = 0;
    virtual void serialize(double& value) = 0;
    virtual void serialize(bool& value) = 0;
    virtual void serialize(std::string& value) = 0;
    
    // Массивы
    template<typename T>
    void serializeArray(std::vector<T>& array) {
        if (isWriting()) {
            uint32_t size = static_cast<uint32_t>(array.size());
            serialize(size);
            for (auto& item : array) {
                serialize(item);
            }
        } else {
            uint32_t size;
            serialize(size);
            array.resize(size);
            for (auto& item : array) {
                serialize(item);
            }
        }
    }
    
    // Операторы для удобного синтаксиса
    template<typename T>
    Serializer& operator&(T& value) {
        serialize(value);
        return *this;
    }
    
    template<typename T>
    Serializer& operator<<(const T& value) {
        if (isWriting()) {
            T mutableValue = value;
            serialize(mutableValue);
        }
        return *this;
    }
    
    template<typename T>
    Serializer& operator>>(T& value) {
        if (isReading()) {
            serialize(value);
        }
        return *this;
    }
};

// Макросы для упрощения сериализации классов
#define SERIALIZE_MEMBER(serializer, member) serializer & member
#define SERIALIZE_MEMBERS(serializer, ...) serializeMembers(serializer, __VA_ARGS__)

// Вспомогательные функции для вариативных шаблонов
template<typename Serializer, typename T>
void serializeMember(Serializer& s, T& member) {
    s & member;
}

template<typename Serializer, typename First, typename... Rest>
void serializeMember(Serializer& s, First& first, Rest&... rest) {
    serializeMember(s, first);
    serializeMember(s, rest...);
}

} // namespace frabgine

#endif // FRABGINE_CORE_SERIALIZATION_SERIALIZER_HPP
