#include "ui_render_node.hpp"
#include "ui_nodes.hpp"
#include "../../render/graphics_device.hpp"
#include <algorithm>
#include <glad/glad.h>
#include <iostream>

namespace arxglue::ui {

UIRenderNode::UIRenderNode() = default;

void UIRenderNode::execute(Context& ctx) {
    std::cout << "[UIRenderNode] execute start" << std::endl;
    if (!ctx.hasState("ui.drawCommands")) {
        static int noCmdCount = 0;
        if (noCmdCount++ % 60 == 0) {
            std::cout << "[UIRenderNode] No draw commands in state" << std::endl;
        }
        std::cout << "[UIRenderNode] execute end (no commands)" << std::endl;
        return;
    }

    auto commands = ctx.getState<std::vector<UIDrawCommand>>("ui.drawCommands");
    if (commands.empty()) {
        static int emptyCount = 0;
        if (emptyCount++ % 60 == 0) {
            std::cout << "[UIRenderNode] Empty draw command list" << std::endl;
        }
        std::cout << "[UIRenderNode] execute end (empty)" << std::endl;
        return;
    }

    static int renderCount = 0;
    if (renderCount++ % 60 == 0) {
        std::cout << "[UIRenderNode] Rendering " << commands.size() << " commands" << std::endl;
    }

    std::sort(commands.begin(), commands.end(),
              [](const UIDrawCommand& a, const UIDrawCommand& b) { return a.zOrder < b.zOrder; });

    int width = ctx.getState<int>("ui.canvasWidth");
    int height = ctx.getState<int>("ui.canvasHeight");
    if (width == 0) width = 800;
    if (height == 0) height = 600;

    std::cout << "[UIRenderNode] Setting viewport " << width << "x" << height << std::endl;
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    std::array<float,16> proj = {
        2.0f / width, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f / height, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f
    };
    std::array<float,16> view = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    for (const auto& c : commands) {
        if (c.mesh && c.material) {
            std::cout << "[UIRenderNode] Drawing command with zOrder=" << c.zOrder << std::endl;
            c.material->apply();
            auto shader = c.material->getShader();
            shader->setUniformMat4("uModel", c.transform.data());
            shader->setUniformMat4("uView", view.data());
            shader->setUniformMat4("uProjection", proj.data());
            c.mesh->draw();
        }
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "[UIRenderNode] OpenGL error: " << err << std::endl;
    }

    ctx.setState("ui.drawCommands", std::vector<UIDrawCommand>{});
    std::cout << "[UIRenderNode] execute end" << std::endl;
}

ComponentMetadata UIRenderNode::getMetadata() const {
    return {"UIRenderNode", {}, {}, false, false};
}

void UIRenderNode::setParameter(const std::string&, const std::any&) {}
std::any UIRenderNode::getParameter(const std::string&) const { return {}; }
void UIRenderNode::serialize(nlohmann::json& j) const { j["type"] = "UIRenderNode"; }
void UIRenderNode::deserialize(const nlohmann::json&) {}

} // namespace arxglue::ui