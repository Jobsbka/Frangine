#pragma once
#include "../../core/node.hpp"
#include "../../core/type_system.hpp"

namespace arxglue {

class ConvertNode : public INode {
public:
    ConvertNode() = default;
    explicit ConvertNode(TypeId from, TypeId to) : m_from(from), m_to(to) {}

    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override;

    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;

    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

protected:
    size_t computeParamsHash() const override;

private:
    TypeId m_from = TypeId::Unknown;
    TypeId m_to = TypeId::Unknown;
};

} // namespace arxglue