#ifndef FRABGINE_CORE_SERIALIZATION_JSONSERIALIZER_HPP
#define FRABGINE_CORE_SERIALIZATION_JSONSERIALIZER_HPP

#include "Serializer.hpp"
#include <nlohmann/json.hpp>
#include <stack>

namespace frabgine {

class JsonSerializer : public Serializer {
private:
    nlohmann::json root_;
    nlohmann::json* current_;
    std::stack<nlohmann::json*> stack_;
    
public:
    explicit JsonSerializer(Mode mode = Mode::Writing);
    
    bool loadFromFile(const std::string& filename);
    bool saveToFile(const std::string& filename) const;
    
    void beginObject(const std::string& name);
    void endObject();
    
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

#endif // FRABGINE_CORE_SERIALIZATION_JSONSERIALIZER_HPP
