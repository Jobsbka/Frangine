// src/nodes/uin/ui_nodes.hpp
#pragma once
#include "../../core/node.hpp"
#include "../../render/mesh.hpp"
#include "../../render/material.hpp"
#include "../../render/texture.hpp"
#include <array>
#include <vector>
#include <memory>
#include <string>

namespace arxglue::ui {

struct UIDrawCommand {
    enum class Type { Rect, Text, Image } type;
    std::shared_ptr<render::Mesh> mesh;
    std::shared_ptr<render::Material> material;
    std::array<float, 16> transform;
    int zOrder = 0;
};

std::array<float, 16> makeTransform2D(const std::array<float, 2>& position,
                                      const std::array<float, 2>& size,
                                      float rotation = 0.0f);

// ========== RectNode ==========
class RectNode : public INode {
public:
    RectNode();
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;
private:
    std::array<float, 2> m_position = {0.0f, 0.0f};
    std::array<float, 2> m_size = {100.0f, 50.0f};
    std::array<float, 4> m_color = {0.8f, 0.8f, 0.8f, 1.0f};
    std::shared_ptr<render::Texture> m_texture;
    std::string m_texturePath;
    int m_zOrder = 0;
    int m_visibleMode = -1;
};

// ========== TextNode ==========
class TextNode : public INode {
public:
    TextNode();
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;
private:
    std::array<float, 2> m_position = {0.0f, 0.0f};
    std::string m_text;
    std::string m_textId;                         // строковый ID (например, "scoreLabel")
    std::array<float, 3> m_color = {1.0f, 1.0f, 1.0f};
    float m_fontSize = 24.0f;
    int m_zOrder = 0;
    int m_visibleMode = -1;
    std::shared_ptr<render::Texture> m_cachedTexture;
};

// ========== ImageNode ==========
class ImageNode : public INode {
public:
    ImageNode();
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;
private:
    std::array<float, 2> m_position = {0.0f, 0.0f};
    std::array<float, 2> m_size = {100.0f, 100.0f};
    std::shared_ptr<render::Texture> m_texture;
    std::string m_texturePath;
    int m_zOrder = 0;
    int m_visibleMode = -1;
};

// ========== ButtonNode ==========
class ButtonNode : public INode {
public:
    ButtonNode();
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;
private:
    std::array<float, 2> m_position = {0.0f, 0.0f};
    std::array<float, 2> m_size = {120.0f, 40.0f};
    std::string m_text;
    std::string m_buttonId;                       // строковый ID (например, "play")
    std::array<float, 4> m_normalColor = {0.7f, 0.7f, 0.7f, 1.0f};
    std::array<float, 4> m_hoverColor = {0.9f, 0.9f, 0.9f, 1.0f};
    std::array<float, 4> m_pressedColor = {0.5f, 0.5f, 0.5f, 1.0f};
    int m_zOrder = 0;
    int m_visibleMode = -1;
    bool m_wasPressed = false;
};

// ========== SliderNode ==========
class SliderNode : public INode {
public:
    SliderNode();
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;
private:
    std::array<float, 2> m_position = {0.0f, 0.0f};
    float m_width = 200.0f;
    float m_minValue = 0.0f;
    float m_maxValue = 1.0f;
    float m_value = 0.5f;
    std::array<float, 4> m_trackColor = {0.3f, 0.3f, 0.3f, 1.0f};
    std::array<float, 4> m_fillColor = {0.2f, 0.6f, 1.0f, 1.0f};
    std::array<float, 4> m_handleColor = {1.0f, 1.0f, 1.0f, 1.0f};
    int m_zOrder = 0;
    int m_visibleMode = -1;
    bool m_dragging = false;
};

// ========== HorizontalLayoutNode ==========
class HorizontalLayoutNode : public INode {
public:
    HorizontalLayoutNode();
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;
private:
    std::array<float, 2> m_position = {0.0f, 0.0f};
    float m_spacing = 10.0f;
    int m_visibleMode = -1;
    std::vector<std::unique_ptr<INode>> m_children;
};

// ========== VerticalLayoutNode ==========
class VerticalLayoutNode : public INode {
public:
    VerticalLayoutNode();
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;
private:
    std::array<float, 2> m_position = {0.0f, 0.0f};
    float m_spacing = 10.0f;
    int m_visibleMode = -1;
    std::vector<std::unique_ptr<INode>> m_children;
};

// ========== CanvasNode ==========
class CanvasNode : public INode {
public:
    CanvasNode();
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;
private:
    int m_width = 800;
    int m_height = 600;
    int m_visibleMode = -1;
    std::vector<std::unique_ptr<INode>> m_children;
};

void registerUINodes();

} // namespace arxglue::ui