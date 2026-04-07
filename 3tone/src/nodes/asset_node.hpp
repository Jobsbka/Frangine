#pragma once
#include "../core/node.hpp"
#include "../assets/asset_manager.hpp"
#include <memory>
#include <string>

namespace arxglue {

class AssetNode : public INode {
public:
    AssetNode() = default;
    explicit AssetNode(const std::string& assetPath, const std::string& assetType = "texture")
        : m_assetPath(assetPath), m_assetType(assetType) {}

    void execute(Context& ctx) override;

    ComponentMetadata getMetadata() const override {
        std::type_index outputType = typeid(void);
        if (m_assetType == "texture") {
            outputType = typeid(std::shared_ptr<TextureAsset>);
        } else if (m_assetType == "mesh") {
            outputType = typeid(std::shared_ptr<MeshAsset>);
        }

        return {"Asset",
                {},
                {{"asset", outputType}},
                true,   // pure
                false}; // not volatile
    }

    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "path") m_assetPath = std::any_cast<std::string>(value);
        else if (name == "type") m_assetType = std::any_cast<std::string>(value);
    }

    std::any getParameter(const std::string& name) const override {
        if (name == "path") return m_assetPath;
        if (name == "type") return m_assetType;
        return {};
    }

    void serialize(nlohmann::json& j) const override {
        j["type"] = "AssetNode";
        j["params"]["path"] = m_assetPath;
        j["params"]["assetType"] = m_assetType;
    }

    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params")) {
            if (j["params"].contains("path")) m_assetPath = j["params"]["path"].get<std::string>();
            if (j["params"].contains("assetType")) m_assetType = j["params"]["assetType"].get<std::string>();
        }
    }

private:
    std::string m_assetPath;
    std::string m_assetType = "texture";
    std::shared_ptr<Asset> m_cachedAsset; // хранится как базовый тип
};

} // namespace arxglue