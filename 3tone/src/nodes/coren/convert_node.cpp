#include "convert_node.hpp"
#include <stdexcept>
#include <functional>

namespace arxglue {

void ConvertNode::execute(Context& ctx) {
    std::any inputVal = getInputValue(ctx, 0);
    if (!inputVal.has_value()) {
        setOutputValue(ctx, 0, std::any{});
        return;
    }

    try {
        std::any converted = TypeSystem::instance().convert(inputVal, m_from, m_to);
        setOutputValue(ctx, 0, converted);
    } catch (const std::exception&) {
        setOutputValue(ctx, 0, std::any{});
    }
}

ComponentMetadata ConvertNode::getMetadata() const {
    std::type_index inputType = TypeSystem::instance().getStdTypeIndex(m_from);
    std::type_index outputType = TypeSystem::instance().getStdTypeIndex(m_to);

    return {
        "Convert",
        {{"in", inputType, true}},
        {{"out", outputType}},
        true,   // pure
        false   // not volatile
    };
}

void ConvertNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "from") {
        m_from = std::any_cast<TypeId>(value);
    } else if (name == "to") {
        m_to = std::any_cast<TypeId>(value);
    }
}

std::any ConvertNode::getParameter(const std::string& name) const {
    if (name == "from") return m_from;
    if (name == "to") return m_to;
    return {};
}

void ConvertNode::serialize(nlohmann::json& j) const {
    j["type"] = "ConvertNode";
    j["params"]["from"] = static_cast<uint64_t>(m_from);
    j["params"]["to"] = static_cast<uint64_t>(m_to);
}

void ConvertNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        if (j["params"].contains("from")) {
            m_from = static_cast<TypeId>(j["params"]["from"].get<uint64_t>());
        }
        if (j["params"].contains("to")) {
            m_to = static_cast<TypeId>(j["params"]["to"].get<uint64_t>());
        }
    }
}

size_t ConvertNode::computeParamsHash() const {
    return std::hash<int>{}(static_cast<int>(m_from)) ^
           (std::hash<int>{}(static_cast<int>(m_to)) << 1);
}

} // namespace arxglue