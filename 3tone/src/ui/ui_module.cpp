#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "ui_module.hpp"
#include "input_manager.hpp"
#include "../nodes/uin/ui_nodes.hpp"
#include "../nodes/uin/ui_render_node.hpp"
#include "../nodes/node_factory.hpp"

namespace arxglue::ui {

void initializeModule() {
    // Регистрация UI узлов
    registerUINodes();

    auto& factory = NodeFactory::instance();
    factory.registerNode("UIRenderNode", []() {
        return std::make_unique<UIRenderNode>();
    });
}

} // namespace arxglue::ui