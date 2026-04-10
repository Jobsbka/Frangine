// src/main.cpp – UI Demo with proper execution order
#include "core/graph.hpp"
#include "core/executor.hpp"
#include "nodes/basic_nodes.hpp"
#include "nodes/node_factory.hpp"
#include "types/type_system.hpp"
#include "render/graphics_device.hpp"
#include "ui/input_manager.hpp"
#include "ui/ui_nodes.hpp"
#include "ui/ui_render_node.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace arxglue;

int main() {
    try {
        std::cout << "=== 3Tone UI Demo Starting ===" << std::endl;
        initBasicTypes();
        registerBasicNodes();
        ui::registerUINodes();

        if (!glfwInit()) return -1;
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        GLFWwindow* window = glfwCreateWindow(800, 600, "3Tone UI Demo", nullptr, nullptr);
        if (!window) { glfwTerminate(); return -1; }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        std::cout << "Window created: 800x600" << std::endl;

        auto& device = render::GraphicsDevice::instance();
        if (!device.initialize(window)) {
            std::cerr << "Failed to initialize GraphicsDevice" << std::endl;
            return -1;
        }
        std::cout << "GraphicsDevice initialized" << std::endl;

        // Построение графа UI
        Graph uiGraph;
        
        // Кнопка
        auto button = std::make_unique<ui::ButtonNode>();
        button->setParameter("position", std::array<float,2>{350.0f, 250.0f});
        button->setParameter("size", std::array<float,2>{100.0f, 40.0f});
        button->setParameter("text", std::string("Click Me"));
        button->setParameter("normalColor", std::array<float,4>{0.7f, 0.7f, 0.7f, 1.0f});
        button->setParameter("hoverColor", std::array<float,4>{0.9f, 0.9f, 0.9f, 1.0f});
        button->setParameter("pressedColor", std::array<float,4>{0.5f, 0.5f, 0.5f, 1.0f});
        NodeId buttonId = uiGraph.addNode(std::move(button));
        std::cout << "Button node added, id=" << buttonId << std::endl;

        // Слайдер
        auto slider = std::make_unique<ui::SliderNode>();
        slider->setParameter("position", std::array<float,2>{300.0f, 320.0f});
        slider->setParameter("width", 200.0f);
        slider->setParameter("minValue", 0.0f);
        slider->setParameter("maxValue", 1.0f);
        slider->setParameter("value", 0.5f);
        NodeId sliderId = uiGraph.addNode(std::move(slider));
        std::cout << "Slider node added, id=" << sliderId << std::endl;

        // Текст (отображает значение слайдера)
        auto text = std::make_unique<ui::TextNode>();
        text->setParameter("position", std::array<float,2>{350.0f, 400.0f});
        text->setParameter("text", std::string("Value: 0.50"));
        text->setParameter("color", std::array<float,3>{1.0f, 1.0f, 1.0f});
        text->setParameter("fontSize", 24.0f);
        NodeId textId = uiGraph.addNode(std::move(text));
        std::cout << "Text node added, id=" << textId << std::endl;

        // Узел рендеринга UI
        auto uiRender = std::make_unique<ui::UIRenderNode>();
        NodeId renderId = uiGraph.addNode(std::move(uiRender));
        std::cout << "UIRenderNode added, id=" << renderId << std::endl;

        // Фиктивные соединения для порядка выполнения
        uiGraph.addConnection({buttonId, 0, renderId, 0});
        uiGraph.addConnection({sliderId, 0, renderId, 0});
        uiGraph.addConnection({textId, 0, renderId, 0});

        Executor executor(0);
        Context ctx;

        ctx.setState("ui.canvasWidth", 800);
        ctx.setState("ui.canvasHeight", 600);

        std::cout << "Entering main loop..." << std::endl;
        int frameCount = 0;
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            
            ui::InputManager::instance().update(window);
            ui::InputManager::instance().writeToContext(ctx);

            device.clear(0.1f, 0.1f, 0.1f, 1.0f);
            
            executor.execute(uiGraph, ctx, {renderId});
            
            std::string clickKey = "ui.button." + std::to_string(buttonId) + ".clicked";
            if (ctx.hasState(clickKey)) {
                bool clicked = ctx.getState<bool>(clickKey);
                if (clicked) {
                    std::cout << "Main: Button clicked!" << std::endl;
                    ctx.setState(clickKey, false);
                }
            }

            std::string sliderKey = "ui.slider." + std::to_string(sliderId) + ".value";
            if (ctx.hasState(sliderKey)) {
                float val = ctx.getState<float>(sliderKey);
                INode* textNode = uiGraph.getNode(textId);
                if (textNode) {
                    std::ostringstream oss;
                    oss << "Value: " << std::fixed << std::setprecision(2) << val;
                    textNode->setParameter("text", oss.str());
                }
            }

            device.swapBuffers();

            if (++frameCount % 60 == 0) {
                std::cout << "Frame " << frameCount << std::endl;
            }
        }

        std::cout << "Main loop exited." << std::endl;
        device.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}