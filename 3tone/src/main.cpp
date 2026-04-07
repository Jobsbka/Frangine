#include "core/graph.hpp"
#include "core/executor.hpp"
#include "nodes/basic_nodes.hpp"
#include "nodes/render_nodes.hpp"
#include "types/type_system.hpp"
#include "render/graphics_device.hpp"
#include <iostream>
#include <exception>
#include <chrono>
#include <fstream>
#include <array>
#include <cmath>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

using namespace arxglue;

// Создание тестового меша (куб) и сохранение в файл
void createTestMeshFile(const std::string& path) {
    // Вершины: позиция (3), нормаль (3)
    std::vector<float> vertices = {
        // Передняя грань
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        // Задняя грань
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        // Левая грань
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        // Правая грань
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
        // Верхняя грань
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
        // Нижняя грань
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f
    };
    std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0,       // передняя
        4, 5, 6, 6, 7, 4,       // задняя
        8, 9,10,10,11, 8,       // левая
       12,13,14,14,15,12,       // правая
       16,17,18,18,19,16,       // верхняя
       20,21,22,22,23,20        // нижняя
    };
    std::vector<int> attribSizes = {3, 3};

    std::ofstream file(path, std::ios::binary);
    uint32_t vcount = static_cast<uint32_t>(vertices.size());
    file.write(reinterpret_cast<const char*>(&vcount), sizeof(vcount));
    file.write(reinterpret_cast<const char*>(vertices.data()), vcount * sizeof(float));
    uint32_t icount = static_cast<uint32_t>(indices.size());
    file.write(reinterpret_cast<const char*>(&icount), sizeof(icount));
    file.write(reinterpret_cast<const char*>(indices.data()), icount * sizeof(uint32_t));
    uint32_t acount = static_cast<uint32_t>(attribSizes.size());
    file.write(reinterpret_cast<const char*>(&acount), sizeof(acount));
    file.write(reinterpret_cast<const char*>(attribSizes.data()), acount * sizeof(int));
}

static std::array<float, 16> identityMatrix() {
    std::array<float, 16> m{};
    m[0] = m[5] = m[10] = m[15] = 1.0f;
    return m;
}

static std::array<float, 16> rotateY(float angle) {
    std::array<float, 16> m = identityMatrix();
    float c = std::cos(angle);
    float s = std::sin(angle);
    m[0] = c;
    m[2] = s;
    m[8] = -s;
    m[10] = c;
    return m;
}

int main() {
    try {
        // Инициализация GLFW
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return -1;
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        GLFWwindow* window = glfwCreateWindow(800, 600, "3Tone Render Test", nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create window" << std::endl;
            glfwTerminate();
            return -1;
        }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        auto& device = render::GraphicsDevice::instance();
        device.initialize(window);

        initBasicTypes();
        registerBasicNodes();
        registerRenderNodes();

        createTestMeshFile("cube.3tm");

        auto scene = std::make_shared<render::Scene>();

        std::ifstream file("cube.3tm", std::ios::binary);
        if (!file) throw std::runtime_error("Failed to open cube.3tm");
        uint32_t vcount;
        file.read(reinterpret_cast<char*>(&vcount), sizeof(vcount));
        std::vector<float> vertices(vcount);
        file.read(reinterpret_cast<char*>(vertices.data()), vcount * sizeof(float));
        uint32_t icount;
        file.read(reinterpret_cast<char*>(&icount), sizeof(icount));
        std::vector<uint32_t> indices(icount);
        file.read(reinterpret_cast<char*>(indices.data()), icount * sizeof(uint32_t));
        uint32_t acount;
        file.read(reinterpret_cast<char*>(&acount), sizeof(acount));
        std::vector<int> attribSizes(acount);
        file.read(reinterpret_cast<char*>(attribSizes.data()), acount * sizeof(int));
        auto mesh = device.createMesh(vertices, indices, attribSizes);

        // Простой шейдер без освещения (чистый цвет)
        const char* vertexSrc = R"(
            #version 450 core
            layout(location = 0) in vec3 aPos;
            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProjection;
            void main() {
                gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
            }
        )";
        const char* fragmentSrc = R"(
            #version 450 core
            uniform vec3 uColor;
            out vec4 FragColor;
            void main() {
                FragColor = vec4(uColor, 1.0);
            }
        )";

        auto shader = device.createShader(vertexSrc, fragmentSrc);
        auto mat = device.createMaterial(shader);
        mat->setParameter("uColor", std::array<float,3>{1.0f, 0.5f, 0.2f}); // оранжевый

        scene->addRenderable(mesh, mat, identityMatrix());

        auto cam = std::make_shared<render::Camera>();
        cam->lookAt({0.0f, 1.0f, 3.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
        const float pi = 3.1415926535f;
        cam->setPerspective(45.0f * pi / 180.0f, 800.0f/600.0f, 0.1f, 100.0f);

        float angle = 0.0f;
        auto lastTime = std::chrono::high_resolution_clock::now();

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            angle += deltaTime * 1.0f;
            if (angle > 2.0f * pi) angle -= 2.0f * pi;

            auto newTransform = rotateY(angle);
            scene = std::make_shared<render::Scene>();
            scene->addRenderable(mesh, mat, newTransform);

            device.clear(0.2f, 0.3f, 0.3f, 1.0f);
            device.setViewport(0, 0, 800, 600);

            render::CommandBuffer cmd;
            cmd.begin();
            cmd.setRenderTarget(nullptr);
            cmd.clear(0.2f, 0.3f, 0.3f, 1.0f);
            cmd.setViewport(0, 0, 800, 600);
            cmd.drawScene(*scene, *cam);
            cmd.end();
            cmd.execute();

            device.swapBuffers();
        }

        device.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();

        std::cout << "Rendering test completed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}