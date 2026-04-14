#pragma once
#include "../../core/node.hpp"
#include "../../render/graphics_device.hpp"
#include "../../render/mesh.hpp"
#include "../../render/material.hpp"
#include "../../render/scene.hpp"
#include "../../render/camera.hpp"
#include "../../render/command_buffer.hpp"
#include "../../assets/asset_manager.hpp"
#include <fstream>
#include <array>

namespace arxglue {

void registerRenderNodes();

// Загрузка меша из файла (бинарный формат .3tm)
class LoadMeshNode : public INode {
public:
    LoadMeshNode(const std::string& path = "") : m_path(path) {}

    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override {
        return {"LoadMesh",
                {},
                {{"mesh", typeid(std::shared_ptr<render::Mesh>)}},
                false, false};
    }
    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "path") m_path = std::any_cast<std::string>(value);
    }
    std::any getParameter(const std::string& name) const override {
        if (name == "path") return m_path;
        return {};
    }
    void serialize(nlohmann::json& j) const override {
        j["type"] = "LoadMesh";
        j["params"]["path"] = m_path;
    }
    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params") && j["params"].contains("path"))
            m_path = j["params"]["path"].get<std::string>();
    }
private:
    std::string m_path;
};

// Создание материала (с шейдерами по умолчанию)
class MaterialNode : public INode {
public:
    MaterialNode() = default;

    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override {
        return {"Material",
                {},
                {{"material", typeid(std::shared_ptr<render::Material>)}},
                false, false};
    }
    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "color") m_color = std::any_cast<std::array<float, 3>>(value);
    }
    std::any getParameter(const std::string& name) const override {
        if (name == "color") return m_color;
        return {};
    }
    void serialize(nlohmann::json& j) const override {
        j["type"] = "Material";
        j["params"]["color"] = {m_color[0], m_color[1], m_color[2]};
    }
    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params") && j["params"].contains("color")) {
            auto arr = j["params"]["color"];
            m_color = {arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>()};
        }
    }
private:
    std::array<float, 3> m_color = {1.0f, 1.0f, 1.0f};
};

// Узел сцены (принимает несколько мешей с материалами)
class SceneNode : public INode {
public:
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override {
        return {"Scene",
                {}, // Входы: несколько (можно добавлять через параметры)
                {{"scene", typeid(std::shared_ptr<render::Scene>)}},
                false, false};
    }
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

private:
    struct RenderableDesc {
        std::shared_ptr<render::Mesh> mesh;
        std::shared_ptr<render::Material> material;
        std::array<float, 16> transform;
    };
    std::vector<RenderableDesc> m_renderables;
};

// Узел камеры
class CameraNode : public INode {
public:
    CameraNode() { updateMatrix(); }

    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override {
        return {"Camera",
                {},
                {{"camera", typeid(std::shared_ptr<render::Camera>)}},
                false, false};
    }
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

private:
    std::array<float, 3> m_eye = {0.0f, 0.0f, 5.0f};
    std::array<float, 3> m_center = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> m_up = {0.0f, 1.0f, 0.0f};
    float m_fov = 45.0f;
    float m_near = 0.1f;
    float m_far = 100.0f;

    void updateMatrix();
};

// Рендеринг сцены в текущий фреймбуфер (обычно экран)
class RenderSceneNode : public INode {
public:
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override {
        return {"RenderScene",
                {{"scene", typeid(std::shared_ptr<render::Scene>)},
                 {"camera", typeid(std::shared_ptr<render::Camera>)}},
                {},
                false, false};
    }
    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "RenderScene"; }
    void deserialize(const nlohmann::json&) override {}
};

// Рендеринг сцены в текстуру
class RenderToTextureNode : public INode {
public:
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override {
        return {"RenderToTexture",
                {{"scene", typeid(std::shared_ptr<render::Scene>)},
                 {"camera", typeid(std::shared_ptr<render::Camera>)}},
                {{"texture", typeid(std::shared_ptr<render::Texture>)}},
                false, false};
    }
    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "width") m_width = std::any_cast<int>(value);
        else if (name == "height") m_height = std::any_cast<int>(value);
    }
    std::any getParameter(const std::string& name) const override {
        if (name == "width") return m_width;
        if (name == "height") return m_height;
        return {};
    }
    void serialize(nlohmann::json& j) const override {
        j["type"] = "RenderToTexture";
        j["params"]["width"] = m_width;
        j["params"]["height"] = m_height;
    }
    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params")) {
            if (j["params"].contains("width")) m_width = j["params"]["width"].get<int>();
            if (j["params"].contains("height")) m_height = j["params"]["height"].get<int>();
        }
    }
private:
    int m_width = 512;
    int m_height = 512;
};

} // namespace arxglue