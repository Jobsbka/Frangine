#pragma once
#include "../../core/node.hpp"
#include "../../assets/asset_manager.hpp"
#include <cstdint>

namespace arxglue {

class CheckerTextureNode : public INode {
public:
    CheckerTextureNode(int width = 256, int height = 256, int cellsX = 8, int cellsY = 8)
        : m_width(width), m_height(height), m_cellsX(cellsX), m_cellsY(cellsY) {}

    void execute(Context& ctx) override;

    ComponentMetadata getMetadata() const override {
        return {"CheckerTexture",
                {},
                {{"texture", typeid(std::shared_ptr<TextureAsset>)}},
                true,   // pure
                false}; // not volatile
    }

    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "width") m_width = std::any_cast<int>(value);
        else if (name == "height") m_height = std::any_cast<int>(value);
        else if (name == "cellsX") m_cellsX = std::any_cast<int>(value);
        else if (name == "cellsY") m_cellsY = std::any_cast<int>(value);
    }

    std::any getParameter(const std::string& name) const override {
        if (name == "width") return m_width;
        if (name == "height") return m_height;
        if (name == "cellsX") return m_cellsX;
        if (name == "cellsY") return m_cellsY;
        return {};
    }

    void serialize(nlohmann::json& j) const override {
        j["type"] = "CheckerTexture";
        j["params"]["width"] = m_width;
        j["params"]["height"] = m_height;
        j["params"]["cellsX"] = m_cellsX;
        j["params"]["cellsY"] = m_cellsY;
    }

    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params")) {
            if (j["params"].contains("width")) m_width = j["params"]["width"].get<int>();
            if (j["params"].contains("height")) m_height = j["params"]["height"].get<int>();
            if (j["params"].contains("cellsX")) m_cellsX = j["params"]["cellsX"].get<int>();
            if (j["params"].contains("cellsY")) m_cellsY = j["params"]["cellsY"].get<int>();
        }
    }

private:
    int m_width;
    int m_height;
    int m_cellsX;
    int m_cellsY;
};

} // namespace arxglue