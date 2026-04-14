#include "ui_nodes.hpp"
#include "../node_factory.hpp"
#include "../../render/graphics_device.hpp"
#include "../coren/text_texture_node.hpp"
#include "../../assets/asset_manager.hpp"
#include "../../core/executor.hpp"
#include <glad/glad.h>
#include <cmath>
#include <iostream>

namespace arxglue::ui {

std::array<float, 16> makeTransform2D(const std::array<float, 2>& position,
                                      const std::array<float, 2>& size,
                                      float rotation) {
    std::array<float, 16> m{};
    float cosR = std::cos(rotation);
    float sinR = std::sin(rotation);
    m[0] = size[0] * cosR;
    m[1] = size[0] * sinR;
    m[4] = -size[1] * sinR;
    m[5] = size[1] * cosR;
    m[10] = 1.0f;
    m[12] = position[0];
    m[13] = position[1];
    m[15] = 1.0f;
    return m;
}

static std::shared_ptr<render::Mesh> createQuadMesh() {
    static std::shared_ptr<render::Mesh> cached;
    if (cached) return cached;
    std::cout << "[UI] Creating quad mesh..." << std::endl;
    std::vector<float> vertices = {
        0.0f, 0.0f, 0.0f,       0.0f,0.0f,1.0f, 0.0f,0.0f,
        1.0f, 0.0f, 0.0f,       0.0f,0.0f,1.0f, 1.0f,0.0f,
        1.0f, 1.0f, 0.0f,       0.0f,0.0f,1.0f, 1.0f,1.0f,
        0.0f, 1.0f, 0.0f,       0.0f,0.0f,1.0f, 0.0f,1.0f
    };
    std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
    std::vector<int> attribSizes = {3, 3, 2};
    auto& device = render::GraphicsDevice::instance();
    cached = device.createMesh(vertices, indices, attribSizes);
    std::cout << "[UI] Quad mesh created." << std::endl;
    return cached;
}

static std::shared_ptr<render::Material> createUIMaterial(const std::array<float, 4>& color,
                                                          std::shared_ptr<render::Texture> texture = nullptr) {
    auto& device = render::GraphicsDevice::instance();
    static std::shared_ptr<render::Shader> uiShader;
    if (!uiShader) {
        std::cout << "[UI] Compiling UI shader..." << std::endl;
        const char* vertexSrc = R"(
            #version 450 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec3 aNormal;
            layout(location = 2) in vec2 aUV;
            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProjection;
            out vec2 vUV;
            void main() {
                vUV = aUV;
                gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
            }
        )";
        const char* fragmentSrc = R"(
            #version 450 core
            in vec2 vUV;
            uniform vec4 uColor;
            uniform sampler2D uTexture;
            uniform bool uUseTexture;
            out vec4 FragColor;
            void main() {
                vec4 texColor = uUseTexture ? texture(uTexture, vUV) : vec4(1.0);
                FragColor = texColor * uColor;
            }
        )";
        uiShader = device.createShader(vertexSrc, fragmentSrc);
        std::cout << "[UI] UI shader compiled (program=" << uiShader->getHandle() << ")" << std::endl;
    }
    auto mat = device.createMaterial(uiShader);
    mat->setParameter("uColor", color);
    mat->setParameter("uUseTexture", texture != nullptr);
    if (texture) mat->setTexture("uTexture", texture);
    return mat;
}

// ---------- RectNode ----------
RectNode::RectNode() = default;

void RectNode::execute(Context& ctx) {
    if (!m_texturePath.empty() && !m_texture) {
        auto asset = AssetManager::instance().loadTexture(m_texturePath);
        if (asset) {
            m_texture = render::GraphicsDevice::instance().createTexture(
                asset->width, asset->height, 4, asset->pixels.data());
        }
    }

    auto mesh = createQuadMesh();
    auto material = createUIMaterial(m_color, m_texture);
    auto transform = makeTransform2D(m_position, m_size);

    UIDrawCommand cmd;
    cmd.type = UIDrawCommand::Type::Rect;
    cmd.mesh = mesh;
    cmd.material = material;
    cmd.transform = transform;
    cmd.zOrder = m_zOrder;

    std::vector<UIDrawCommand> cmds;
    if (ctx.hasState("ui.drawCommands")) {
        cmds = ctx.getState<std::vector<UIDrawCommand>>("ui.drawCommands");
    }
    cmds.push_back(cmd);
    ctx.setState("ui.drawCommands", cmds);
}

ComponentMetadata RectNode::getMetadata() const {
    return {"RectNode", {}, {}, false, false};
}

void RectNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "position") m_position = std::any_cast<std::array<float,2>>(value);
    else if (name == "size") m_size = std::any_cast<std::array<float,2>>(value);
    else if (name == "color") m_color = std::any_cast<std::array<float,4>>(value);
    else if (name == "texturePath") m_texturePath = std::any_cast<std::string>(value);
    else if (name == "zOrder") m_zOrder = std::any_cast<int>(value);
}

std::any RectNode::getParameter(const std::string& name) const {
    if (name == "position") return m_position;
    if (name == "size") return m_size;
    if (name == "color") return m_color;
    if (name == "texturePath") return m_texturePath;
    if (name == "zOrder") return m_zOrder;
    return {};
}

void RectNode::serialize(nlohmann::json& j) const {
    j["type"] = "RectNode";
    j["params"]["position"] = {m_position[0], m_position[1]};
    j["params"]["size"] = {m_size[0], m_size[1]};
    j["params"]["color"] = {m_color[0], m_color[1], m_color[2], m_color[3]};
    j["params"]["texturePath"] = m_texturePath;
    j["params"]["zOrder"] = m_zOrder;
}

void RectNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        auto& p = j["params"];
        if (p.contains("position")) { auto arr = p["position"]; m_position = {arr[0], arr[1]}; }
        if (p.contains("size")) { auto arr = p["size"]; m_size = {arr[0], arr[1]}; }
        if (p.contains("color")) { auto arr = p["color"]; m_color = {arr[0], arr[1], arr[2], arr[3]}; }
        if (p.contains("texturePath")) m_texturePath = p["texturePath"];
        if (p.contains("zOrder")) m_zOrder = p["zOrder"];
    }
}

// ---------- TextNode ----------
TextNode::TextNode() = default;

void TextNode::execute(Context& ctx) {
    static int frameCount = 0;
    if (frameCount++ % 60 == 0) {
        std::cout << "[TextNode] execute, pos=(" << m_position[0] << "," << m_position[1] << ") text=\"" << m_text << "\"" << std::endl;
    }
    if (m_text.empty()) return;

    if (!m_cachedTexture) {
        std::cout << "[TextNode] Generating texture for text: " << m_text << std::endl;
        TextTextureNode texNode(static_cast<int>(m_fontSize * m_text.length() * 0.6f),
                                static_cast<int>(m_fontSize * 1.5f),
                                m_text,
                                static_cast<uint8_t>(m_color[0] * 255),
                                static_cast<uint8_t>(m_color[1] * 255),
                                static_cast<uint8_t>(m_color[2] * 255));
        Context dummyCtx;
        texNode.execute(dummyCtx);
        auto asset = std::any_cast<std::shared_ptr<TextureAsset>>(dummyCtx.output);
        if (asset) {
            m_cachedTexture = render::GraphicsDevice::instance().createTexture(
                asset->width, asset->height, 4, asset->pixels.data());
            std::cout << "[TextNode] Texture created: " << asset->width << "x" << asset->height << std::endl;
        } else {
            std::cerr << "[TextNode] Failed to create texture asset!" << std::endl;
        }
    }

    if (m_cachedTexture) {
        auto mesh = createQuadMesh();
        std::array<float,2> size = { static_cast<float>(m_cachedTexture->getWidth()),
                                     static_cast<float>(m_cachedTexture->getHeight()) };
        auto material = createUIMaterial({1.0f,1.0f,1.0f,1.0f}, m_cachedTexture);
        auto transform = makeTransform2D(m_position, size);

        UIDrawCommand cmd;
        cmd.type = UIDrawCommand::Type::Text;
        cmd.mesh = mesh;
        cmd.material = material;
        cmd.transform = transform;
        cmd.zOrder = m_zOrder;

        std::vector<UIDrawCommand> cmds;
        if (ctx.hasState("ui.drawCommands")) {
            cmds = ctx.getState<std::vector<UIDrawCommand>>("ui.drawCommands");
        }
        cmds.push_back(cmd);
        ctx.setState("ui.drawCommands", cmds);
    }
}

ComponentMetadata TextNode::getMetadata() const {
    return {"TextNode", {}, {}, false, false};
}

void TextNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "position") m_position = std::any_cast<std::array<float,2>>(value);
    else if (name == "text") { m_text = std::any_cast<std::string>(value); m_cachedTexture.reset(); }
    else if (name == "color") m_color = std::any_cast<std::array<float,3>>(value);
    else if (name == "fontSize") m_fontSize = std::any_cast<float>(value);
    else if (name == "zOrder") m_zOrder = std::any_cast<int>(value);
}

std::any TextNode::getParameter(const std::string& name) const {
    if (name == "position") return m_position;
    if (name == "text") return m_text;
    if (name == "color") return m_color;
    if (name == "fontSize") return m_fontSize;
    if (name == "zOrder") return m_zOrder;
    return {};
}

void TextNode::serialize(nlohmann::json& j) const {
    j["type"] = "TextNode";
    j["params"]["position"] = {m_position[0], m_position[1]};
    j["params"]["text"] = m_text;
    j["params"]["color"] = {m_color[0], m_color[1], m_color[2]};
    j["params"]["fontSize"] = m_fontSize;
    j["params"]["zOrder"] = m_zOrder;
}

void TextNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        auto& p = j["params"];
        if (p.contains("position")) { auto arr = p["position"]; m_position = {arr[0], arr[1]}; }
        if (p.contains("text")) m_text = p["text"];
        if (p.contains("color")) { auto arr = p["color"]; m_color = {arr[0], arr[1], arr[2]}; }
        if (p.contains("fontSize")) m_fontSize = p["fontSize"];
        if (p.contains("zOrder")) m_zOrder = p["zOrder"];
    }
}

// ---------- ImageNode ----------
ImageNode::ImageNode() = default;

void ImageNode::execute(Context& ctx) {
    if (!m_texturePath.empty() && !m_texture) {
        auto asset = AssetManager::instance().loadTexture(m_texturePath);
        if (asset) {
            m_texture = render::GraphicsDevice::instance().createTexture(
                asset->width, asset->height, 4, asset->pixels.data());
        }
    }
    if (!m_texture) return;

    auto mesh = createQuadMesh();
    auto material = createUIMaterial({1.0f,1.0f,1.0f,1.0f}, m_texture);
    auto transform = makeTransform2D(m_position, m_size);

    UIDrawCommand cmd;
    cmd.type = UIDrawCommand::Type::Image;
    cmd.mesh = mesh;
    cmd.material = material;
    cmd.transform = transform;
    cmd.zOrder = m_zOrder;

    std::vector<UIDrawCommand> cmds;
    if (ctx.hasState("ui.drawCommands")) {
        cmds = ctx.getState<std::vector<UIDrawCommand>>("ui.drawCommands");
    }
    cmds.push_back(cmd);
    ctx.setState("ui.drawCommands", cmds);
}

ComponentMetadata ImageNode::getMetadata() const {
    return {"ImageNode", {}, {}, false, false};
}

void ImageNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "position") m_position = std::any_cast<std::array<float,2>>(value);
    else if (name == "size") m_size = std::any_cast<std::array<float,2>>(value);
    else if (name == "texturePath") { m_texturePath = std::any_cast<std::string>(value); m_texture.reset(); }
    else if (name == "zOrder") m_zOrder = std::any_cast<int>(value);
}

std::any ImageNode::getParameter(const std::string& name) const {
    if (name == "position") return m_position;
    if (name == "size") return m_size;
    if (name == "texturePath") return m_texturePath;
    if (name == "zOrder") return m_zOrder;
    return {};
}

void ImageNode::serialize(nlohmann::json& j) const {
    j["type"] = "ImageNode";
    j["params"]["position"] = {m_position[0], m_position[1]};
    j["params"]["size"] = {m_size[0], m_size[1]};
    j["params"]["texturePath"] = m_texturePath;
    j["params"]["zOrder"] = m_zOrder;
}

void ImageNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        auto& p = j["params"];
        if (p.contains("position")) { auto arr = p["position"]; m_position = {arr[0], arr[1]}; }
        if (p.contains("size")) { auto arr = p["size"]; m_size = {arr[0], arr[1]}; }
        if (p.contains("texturePath")) m_texturePath = p["texturePath"];
        if (p.contains("zOrder")) m_zOrder = p["zOrder"];
    }
}

// ---------- ButtonNode ----------
ButtonNode::ButtonNode() = default;

void ButtonNode::execute(Context& ctx) {
    static int frameCount = 0;
    if (frameCount++ % 60 == 0) {
        std::cout << "[ButtonNode] execute, pos=(" << m_position[0] << "," << m_position[1] << ")" << std::endl;
    }
    float mouseX = ctx.getState<float>("input.mouseX");
    float mouseY = ctx.getState<float>("input.mouseY");
    bool mouseLeft = ctx.getState<bool>("input.mouseLeft");

    bool hovered = (mouseX >= m_position[0] && mouseX <= m_position[0] + m_size[0] &&
                    mouseY >= m_position[1] && mouseY <= m_position[1] + m_size[1]);

    std::array<float,4> color = m_normalColor;
    if (hovered) {
        color = mouseLeft ? m_pressedColor : m_hoverColor;
    }

    bool pressed = hovered && mouseLeft;
    bool clicked = m_wasPressed && !pressed;
    m_wasPressed = pressed;

    if (clicked) {
        ctx.setState("ui.button." + std::to_string(getId()) + ".clicked", true);
        std::cout << "[ButtonNode] Clicked!" << std::endl;
    }

    auto mesh = createQuadMesh();
    auto material = createUIMaterial(color, nullptr);
    auto transform = makeTransform2D(m_position, m_size);

    UIDrawCommand cmd;
    cmd.type = UIDrawCommand::Type::Rect;
    cmd.mesh = mesh;
    cmd.material = material;
    cmd.transform = transform;
    cmd.zOrder = m_zOrder;

    std::vector<UIDrawCommand> cmds;
    if (ctx.hasState("ui.drawCommands")) {
        cmds = ctx.getState<std::vector<UIDrawCommand>>("ui.drawCommands");
    }
    cmds.push_back(cmd);
    ctx.setState("ui.drawCommands", cmds);
}

ComponentMetadata ButtonNode::getMetadata() const {
    return {"ButtonNode", {}, {{"clicked", typeid(bool)}}, false, true};
}

void ButtonNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "position") m_position = std::any_cast<std::array<float,2>>(value);
    else if (name == "size") m_size = std::any_cast<std::array<float,2>>(value);
    else if (name == "text") m_text = std::any_cast<std::string>(value);
    else if (name == "normalColor") m_normalColor = std::any_cast<std::array<float,4>>(value);
    else if (name == "hoverColor") m_hoverColor = std::any_cast<std::array<float,4>>(value);
    else if (name == "pressedColor") m_pressedColor = std::any_cast<std::array<float,4>>(value);
    else if (name == "zOrder") m_zOrder = std::any_cast<int>(value);
}

std::any ButtonNode::getParameter(const std::string& name) const {
    if (name == "position") return m_position;
    if (name == "size") return m_size;
    if (name == "text") return m_text;
    if (name == "normalColor") return m_normalColor;
    if (name == "hoverColor") return m_hoverColor;
    if (name == "pressedColor") return m_pressedColor;
    if (name == "zOrder") return m_zOrder;
    return {};
}

void ButtonNode::serialize(nlohmann::json& j) const {
    j["type"] = "ButtonNode";
    j["params"]["position"] = {m_position[0], m_position[1]};
    j["params"]["size"] = {m_size[0], m_size[1]};
    j["params"]["text"] = m_text;
    j["params"]["normalColor"] = {m_normalColor[0], m_normalColor[1], m_normalColor[2], m_normalColor[3]};
    j["params"]["hoverColor"] = {m_hoverColor[0], m_hoverColor[1], m_hoverColor[2], m_hoverColor[3]};
    j["params"]["pressedColor"] = {m_pressedColor[0], m_pressedColor[1], m_pressedColor[2], m_pressedColor[3]};
    j["params"]["zOrder"] = m_zOrder;
}

void ButtonNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        auto& p = j["params"];
        if (p.contains("position")) { auto arr = p["position"]; m_position = {arr[0], arr[1]}; }
        if (p.contains("size")) { auto arr = p["size"]; m_size = {arr[0], arr[1]}; }
        if (p.contains("text")) m_text = p["text"];
        if (p.contains("normalColor")) { auto arr = p["normalColor"]; m_normalColor = {arr[0], arr[1], arr[2], arr[3]}; }
        if (p.contains("hoverColor")) { auto arr = p["hoverColor"]; m_hoverColor = {arr[0], arr[1], arr[2], arr[3]}; }
        if (p.contains("pressedColor")) { auto arr = p["pressedColor"]; m_pressedColor = {arr[0], arr[1], arr[2], arr[3]}; }
        if (p.contains("zOrder")) m_zOrder = p["zOrder"];
    }
}

// ---------- SliderNode ----------
SliderNode::SliderNode() = default;

void SliderNode::execute(Context& ctx) {
    static int frameCount = 0;
    if (frameCount++ % 60 == 0) {
        std::cout << "[SliderNode] execute, pos=(" << m_position[0] << "," << m_position[1] << ") value=" << m_value << std::endl;
    }
    float mouseX = ctx.getState<float>("input.mouseX");
    float mouseY = ctx.getState<float>("input.mouseY");
    bool mouseLeft = ctx.getState<bool>("input.mouseLeft");

    float trackHeight = 10.0f;
    float handleRadius = 12.0f;
    float trackY = m_position[1] + handleRadius - trackHeight/2;
    float handleX = m_position[0] + (m_value - m_minValue) / (m_maxValue - m_minValue) * m_width;

    bool overTrack = (mouseX >= m_position[0] && mouseX <= m_position[0] + m_width &&
                      mouseY >= trackY && mouseY <= trackY + trackHeight);
    bool overHandle = (std::hypot(mouseX - handleX, mouseY - (m_position[1] + handleRadius)) <= handleRadius);

    if (mouseLeft && (overTrack || overHandle) && !m_dragging) {
        m_dragging = true;
    }
    if (!mouseLeft) m_dragging = false;

    if (m_dragging) {
        float t = (mouseX - m_position[0]) / m_width;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        m_value = m_minValue + t * (m_maxValue - m_minValue);
        ctx.setState("ui.slider." + std::to_string(getId()) + ".value", m_value);
    }

    auto mesh = createQuadMesh();
    std::array<float,2> trackSize = {m_width, trackHeight};
    auto trackMat = createUIMaterial(m_trackColor, nullptr);
    auto trackTransform = makeTransform2D({m_position[0], trackY}, trackSize);
    UIDrawCommand trackCmd{UIDrawCommand::Type::Rect, mesh, trackMat, trackTransform, m_zOrder};

    float fillWidth = (m_value - m_minValue) / (m_maxValue - m_minValue) * m_width;
    auto fillMat = createUIMaterial(m_fillColor, nullptr);
    auto fillTransform = makeTransform2D({m_position[0], trackY}, {fillWidth, trackHeight});
    UIDrawCommand fillCmd{UIDrawCommand::Type::Rect, mesh, fillMat, fillTransform, m_zOrder+1};

    std::array<float,2> handleSize = {handleRadius*2, handleRadius*2};
    auto handleMat = createUIMaterial(m_handleColor, nullptr);
    auto handleTransform = makeTransform2D({handleX - handleRadius, m_position[1]}, handleSize);
    UIDrawCommand handleCmd{UIDrawCommand::Type::Rect, mesh, handleMat, handleTransform, m_zOrder+2};

    std::vector<UIDrawCommand> cmds;
    if (ctx.hasState("ui.drawCommands")) {
        cmds = ctx.getState<std::vector<UIDrawCommand>>("ui.drawCommands");
    }
    cmds.push_back(trackCmd);
    cmds.push_back(fillCmd);
    cmds.push_back(handleCmd);
    ctx.setState("ui.drawCommands", cmds);
}

ComponentMetadata SliderNode::getMetadata() const {
    return {"SliderNode", {}, {{"value", typeid(float)}}, false, true};
}

void SliderNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "position") m_position = std::any_cast<std::array<float,2>>(value);
    else if (name == "width") m_width = std::any_cast<float>(value);
    else if (name == "minValue") m_minValue = std::any_cast<float>(value);
    else if (name == "maxValue") m_maxValue = std::any_cast<float>(value);
    else if (name == "value") m_value = std::any_cast<float>(value);
    else if (name == "trackColor") m_trackColor = std::any_cast<std::array<float,4>>(value);
    else if (name == "fillColor") m_fillColor = std::any_cast<std::array<float,4>>(value);
    else if (name == "handleColor") m_handleColor = std::any_cast<std::array<float,4>>(value);
    else if (name == "zOrder") m_zOrder = std::any_cast<int>(value);
}

std::any SliderNode::getParameter(const std::string& name) const {
    if (name == "position") return m_position;
    if (name == "width") return m_width;
    if (name == "minValue") return m_minValue;
    if (name == "maxValue") return m_maxValue;
    if (name == "value") return m_value;
    if (name == "trackColor") return m_trackColor;
    if (name == "fillColor") return m_fillColor;
    if (name == "handleColor") return m_handleColor;
    if (name == "zOrder") return m_zOrder;
    return {};
}

void SliderNode::serialize(nlohmann::json& j) const {
    j["type"] = "SliderNode";
    j["params"]["position"] = {m_position[0], m_position[1]};
    j["params"]["width"] = m_width;
    j["params"]["minValue"] = m_minValue;
    j["params"]["maxValue"] = m_maxValue;
    j["params"]["value"] = m_value;
    j["params"]["trackColor"] = {m_trackColor[0], m_trackColor[1], m_trackColor[2], m_trackColor[3]};
    j["params"]["fillColor"] = {m_fillColor[0], m_fillColor[1], m_fillColor[2], m_fillColor[3]};
    j["params"]["handleColor"] = {m_handleColor[0], m_handleColor[1], m_handleColor[2], m_handleColor[3]};
    j["params"]["zOrder"] = m_zOrder;
}

void SliderNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        auto& p = j["params"];
        if (p.contains("position")) { auto arr = p["position"]; m_position = {arr[0], arr[1]}; }
        if (p.contains("width")) m_width = p["width"];
        if (p.contains("minValue")) m_minValue = p["minValue"];
        if (p.contains("maxValue")) m_maxValue = p["maxValue"];
        if (p.contains("value")) m_value = p["value"];
        if (p.contains("trackColor")) { auto arr = p["trackColor"]; m_trackColor = {arr[0], arr[1], arr[2], arr[3]}; }
        if (p.contains("fillColor")) { auto arr = p["fillColor"]; m_fillColor = {arr[0], arr[1], arr[2], arr[3]}; }
        if (p.contains("handleColor")) { auto arr = p["handleColor"]; m_handleColor = {arr[0], arr[1], arr[2], arr[3]}; }
        if (p.contains("zOrder")) m_zOrder = p["zOrder"];
    }
}

// ---------- HorizontalLayoutNode ----------
HorizontalLayoutNode::HorizontalLayoutNode() = default;

void HorizontalLayoutNode::execute(Context& ctx) {
    float x = m_position[0];
    for (auto& child : m_children) {
        child->setParameter("position", std::array<float,2>{x, m_position[1]});
        child->execute(ctx);
        std::array<float,2> size = std::any_cast<std::array<float,2>>(child->getParameter("size"));
        x += size[0] + m_spacing;
    }
}

ComponentMetadata HorizontalLayoutNode::getMetadata() const {
    return {"HorizontalLayoutNode", {}, {}, false, false};
}

void HorizontalLayoutNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "position") m_position = std::any_cast<std::array<float,2>>(value);
    else if (name == "spacing") m_spacing = std::any_cast<float>(value);
}

std::any HorizontalLayoutNode::getParameter(const std::string& name) const {
    if (name == "position") return m_position;
    if (name == "spacing") return m_spacing;
    return {};
}

void HorizontalLayoutNode::serialize(nlohmann::json& j) const {
    j["type"] = "HorizontalLayoutNode";
    j["params"]["position"] = {m_position[0], m_position[1]};
    j["params"]["spacing"] = m_spacing;
}

void HorizontalLayoutNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        auto& p = j["params"];
        if (p.contains("position")) { auto arr = p["position"]; m_position = {arr[0], arr[1]}; }
        if (p.contains("spacing")) m_spacing = p["spacing"];
    }
}

// ---------- VerticalLayoutNode ----------
VerticalLayoutNode::VerticalLayoutNode() = default;

void VerticalLayoutNode::execute(Context& ctx) {
    float y = m_position[1];
    for (auto& child : m_children) {
        child->setParameter("position", std::array<float,2>{m_position[0], y});
        child->execute(ctx);
        std::array<float,2> size = std::any_cast<std::array<float,2>>(child->getParameter("size"));
        y += size[1] + m_spacing;
    }
}

ComponentMetadata VerticalLayoutNode::getMetadata() const {
    return {"VerticalLayoutNode", {}, {}, false, false};
}

void VerticalLayoutNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "position") m_position = std::any_cast<std::array<float,2>>(value);
    else if (name == "spacing") m_spacing = std::any_cast<float>(value);
}

std::any VerticalLayoutNode::getParameter(const std::string& name) const {
    if (name == "position") return m_position;
    if (name == "spacing") return m_spacing;
    return {};
}

void VerticalLayoutNode::serialize(nlohmann::json& j) const {
    j["type"] = "VerticalLayoutNode";
    j["params"]["position"] = {m_position[0], m_position[1]};
    j["params"]["spacing"] = m_spacing;
}

void VerticalLayoutNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        auto& p = j["params"];
        if (p.contains("position")) { auto arr = p["position"]; m_position = {arr[0], arr[1]}; }
        if (p.contains("spacing")) m_spacing = p["spacing"];
    }
}

// ---------- CanvasNode ----------
CanvasNode::CanvasNode() = default;

void CanvasNode::execute(Context& ctx) {
    ctx.setState("ui.canvasWidth", m_width);
    ctx.setState("ui.canvasHeight", m_height);
    for (auto& child : m_children) {
        child->execute(ctx);
    }
}

ComponentMetadata CanvasNode::getMetadata() const {
    return {"CanvasNode", {}, {}, false, false};
}

void CanvasNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "width") m_width = std::any_cast<int>(value);
    else if (name == "height") m_height = std::any_cast<int>(value);
}

std::any CanvasNode::getParameter(const std::string& name) const {
    if (name == "width") return m_width;
    if (name == "height") return m_height;
    return {};
}

void CanvasNode::serialize(nlohmann::json& j) const {
    j["type"] = "CanvasNode";
    j["params"]["width"] = m_width;
    j["params"]["height"] = m_height;
}

void CanvasNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        auto& p = j["params"];
        if (p.contains("width")) m_width = p["width"];
        if (p.contains("height")) m_height = p["height"];
    }
}

// ---------- Регистрация ----------
void registerUINodes() {
    auto& factory = NodeFactory::instance();
    factory.registerNode("RectNode", []() { return std::make_unique<RectNode>(); });
    factory.registerNode("TextNode", []() { return std::make_unique<TextNode>(); });
    factory.registerNode("ImageNode", []() { return std::make_unique<ImageNode>(); });
    factory.registerNode("ButtonNode", []() { return std::make_unique<ButtonNode>(); });
    factory.registerNode("SliderNode", []() { return std::make_unique<SliderNode>(); });
    factory.registerNode("HorizontalLayoutNode", []() { return std::make_unique<HorizontalLayoutNode>(); });
    factory.registerNode("VerticalLayoutNode", []() { return std::make_unique<VerticalLayoutNode>(); });
    factory.registerNode("CanvasNode", []() { return std::make_unique<CanvasNode>(); });
}

} // namespace arxglue::ui