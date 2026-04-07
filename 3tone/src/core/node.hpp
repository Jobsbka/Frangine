#pragma once
#include "../arxglue.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>
#include <iostream>

namespace arxglue {

class INode {
public:
    virtual ~INode() = default;
    virtual void execute(Context& ctx) = 0;
    virtual ComponentMetadata getMetadata() const = 0;
    virtual void setParameter(const std::string& name, const std::any& value) = 0;
    virtual std::any getParameter(const std::string& name) const = 0;
    virtual void serialize(nlohmann::json& j) const = 0;
    virtual void deserialize(const nlohmann::json& j) = 0;

    void setId(NodeId id) { m_id = id; }
    NodeId getId() const { return m_id; }

    bool isDirty() const { return m_dirty; }
    void setDirty(bool dirty) { m_dirty = dirty; }

    void setCachedOutput(const std::any& output) { m_cachedOutput = output; }
    std::any getCachedOutput() const { return m_cachedOutput; }

    // ---- ИСПРАВЛЕНО: используем индекс порта, а не имя ----
    std::any getInputValue(Context& ctx, int portIndex) const {
        const auto& inputs = getMetadata().inputs;
        if (portIndex >= 0 && portIndex < static_cast<int>(inputs.size())) {
            std::string key = "in_" + std::to_string(m_id) + "_" + std::to_string(portIndex);
            auto it = ctx.state.find(key);
            if (it != ctx.state.end()) return it->second;
        }
        return {};
    }

    void setOutputValue(Context& ctx, int portIndex, const std::any& value) {
        const auto& outputs = getMetadata().outputs;
        if (portIndex >= 0 && portIndex < static_cast<int>(outputs.size())) {
            std::string key = "out_" + std::to_string(m_id) + "_" + std::to_string(portIndex);
            ctx.state[key] = value;
        }
        ctx.output = value;
    }

private:
    NodeId m_id = 0;
    bool m_dirty = true;
    std::any m_cachedOutput;
};

} // namespace arxglue