#ifndef FRABGINE_CORE_SERIALIZATION_BINARYSERIALIZER_HPP
#define FRABGINE_CORE_SERIALIZATION_BINARYSERIALIZER_HPP

#include "Serializer.hpp"
#include <vector>
#include <cstdint>

namespace frabgine {

class BinarySerializer : public Serializer {
private:
    std::vector<uint8_t> buffer_;
    size_t position_ = 0;
    
    void ensureSize(size_t bytes);
    
public:
    explicit BinarySerializer(Mode mode = Mode::Writing);
    
    bool loadFromFile(const std::string& filename);
    bool saveToFile(const std::string& filename) const;
    
    const std::vector<uint8_t>& getBuffer() const { return buffer_; }
    size_t getPosition() const { return position_; }
    
    void serialize(int8_t& value) override;
    void serialize(uint8_t& value) override;
    void serialize(int16_t& value) override;
    void serialize(uint16_t& value) override;
    void serialize(int32_t& value) override;
    void serialize(uint32_t& value) override;
    void serialize(int64_t& value) override;
    void serialize(uint64_t& value) override;
    void serialize(float& value) override;
    void serialize(double& value) override;
    void serialize(bool& value) override;
    void serialize(std::string& value) override;
};

} // namespace frabgine

#endif // FRABGINE_CORE_SERIALIZATION_BINARYSERIALIZER_HPP
