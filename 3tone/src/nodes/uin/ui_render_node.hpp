#pragma once
#include "../../core/node.hpp"

namespace arxglue::ui {

class UIRenderNode : public INode {
public:
    UIRenderNode();
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;
};

} // namespace arxglue::ui