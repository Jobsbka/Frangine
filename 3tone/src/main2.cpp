// src/main.cpp
#include "core/graph.hpp"
#include "core/executor.hpp"
#include "nodes/basic_nodes.hpp"
#include "nodes/render_nodes.hpp"
#include "nodes/ga_nodes.hpp"
#include "types/type_system.hpp"
#include "render/graphics_device.hpp"
#include <iostream>
#include <exception>
#include <chrono>
#include <fstream>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

using namespace arxglue;
using namespace arxglue::ga;

static void checkGLError(const char* file, int line) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL error 0x" << std::hex << err << std::dec
                  << " at " << file << ":" << line << std::endl;
    }
}
#define CHECK_GL() checkGLError(__FILE__, __LINE__)

void createTestMeshFile(const std::string& path) {
    std::cout << "Creating test mesh: " << path << std::endl;
    std::vector<float> vertices = {
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f
    };
    std::vector<uint32_t> indices = {
        0,1,2, 2,3,0,
        4,5,6, 6,7,4,
        8,9,10, 10,11,8,
        12,13,14, 14,15,12,
        16,17,18, 18,19,16,
        20,21,22, 22,23,20
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
    std::cout << "Mesh created successfully." << std::endl;
}

std::shared_ptr<render::Shader> createDefaultShader() {
    const char* vertexSrc = R"(
        #version 450 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;
        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProjection;
        out vec3 vNormal;
        out vec3 vFragPos;
        void main() {
            vFragPos = vec3(uModel * vec4(aPos, 1.0));
            vNormal = mat3(transpose(inverse(uModel))) * aNormal;
            gl_Position = uProjection * uView * vec4(vFragPos, 1.0);
        }
    )";
    const char* fragmentSrc = R"(
        #version 450 core
        in vec3 vNormal;
        in vec3 vFragPos;
        uniform vec3 uColor;
        out vec4 FragColor;
        void main() {
            vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
            vec3 normal = normalize(vNormal);
            float diff = max(dot(normal, lightDir), 0.0);
            vec3 ambient = vec3(0.1);
            vec3 result = (ambient + diff) * uColor;
            FragColor = vec4(result, 1.0);
        }
    )";
    auto& device = render::GraphicsDevice::instance();
    std::cout << "Compiling shader..." << std::endl;
    auto shader = device.createShader(vertexSrc, fragmentSrc);
    std::cout << "Shader compiled successfully." << std::endl;
    return shader;
}

void GLAPIENTRY glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                GLsizei length, const GLchar* message, const void* userParam) {
    std::cerr << "GL Debug: " << message << std::endl;
}

struct RigidBody {
    std::array<float, 3> position;
    std::array<float, 3> velocity;
    float radius = 0.866f;
    float mass = 1.0f;
};

// Простая камера с управлением мышью и WASD
struct FlyCamera {
    std::array<float, 3> position = {5.0f, 5.0f, 10.0f};
    float yaw   = -90.0f;   // градусы, направление взгляда
    float pitch = 0.0f;
    float fov   = 45.0f;
    float nearPlane = 0.1f;
    float farPlane  = 100.0f;
    
    float movementSpeed = 5.0f;
    float mouseSensitivity = 0.1f;
    
    void processKeyboard(GLFWwindow* window, float deltaTime) {
        float velocity = movementSpeed * deltaTime;
        std::array<float,3> front = getFront();
        std::array<float,3> right = getRight();
        
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            for(int i=0;i<3;++i) position[i] += front[i] * velocity;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            for(int i=0;i<3;++i) position[i] -= front[i] * velocity;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            for(int i=0;i<3;++i) position[i] -= right[i] * velocity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            for(int i=0;i<3;++i) position[i] += right[i] * velocity;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            position[1] -= velocity;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            position[1] += velocity;
    }
    
    void processMouseMovement(double xoffset, double yoffset) {
        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;
        
        yaw   += static_cast<float>(xoffset);
        pitch += static_cast<float>(yoffset);
        
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    }
    
    std::array<float,3> getFront() const {
        float yawRad = yaw * 3.1415926535f / 180.0f;
        float pitchRad = pitch * 3.1415926535f / 180.0f;
        return {
            std::cos(yawRad) * std::cos(pitchRad),
            std::sin(pitchRad),
            std::sin(yawRad) * std::cos(pitchRad)
        };
    }
    
    std::array<float,3> getRight() const {
        float yawRad = yaw * 3.1415926535f / 180.0f;
        return {
            std::sin(yawRad),
            0.0f,
            -std::cos(yawRad)
        };
    }
    
    void applyToRenderer(std::shared_ptr<render::Camera> cam, float aspect) {
        auto front = getFront();
        std::array<float,3> center = {
            position[0] + front[0],
            position[1] + front[1],
            position[2] + front[2]
        };
        cam->lookAt(position, center, {0.0f, 1.0f, 0.0f});
        cam->setPerspective(fov * 3.1415926535f / 180.0f, aspect, nearPlane, farPlane);
    }
};

int main() {
    try {
        std::cout << "Initializing GLFW..." << std::endl;
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return -1;
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
        GLFWwindow* window = glfwCreateWindow(1920, 1080, "3Tone - GA Physics Demo", nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create window" << std::endl;
            glfwTerminate();
            return -1;
        }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        glfwShowWindow(window);
        
        // Захват мыши (скрываем курсор и захватываем для свободного вращения)
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        
        std::cout << "GLFW initialized." << std::endl;

        auto& device = render::GraphicsDevice::instance();
        if (!device.initialize(window)) {
            std::cerr << "Failed to initialize graphics device" << std::endl;
            return -1;
        }
        std::cout << "Graphics device initialized." << std::endl;

        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(glDebugCallback, nullptr);
        CHECK_GL();

        std::cout << "Initializing type system and nodes..." << std::endl;
        initBasicTypes();
        registerBasicNodes();
        registerRenderNodes();
        registerGANodes();
        std::cout << "Nodes registered." << std::endl;

        createTestMeshFile("cube.3tm");

        std::cout << "Loading mesh..." << std::endl;
        std::ifstream file("cube.3tm", std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open cube.3tm" << std::endl;
            return -1;
        }
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
        std::cout << "Mesh loaded." << std::endl;
        CHECK_GL();

        auto shader = createDefaultShader();
        auto matRed    = device.createMaterial(shader); matRed->setParameter("uColor", std::array<float,3>{1.0f, 0.2f, 0.2f});
        auto matGreen  = device.createMaterial(shader); matGreen->setParameter("uColor", std::array<float,3>{0.2f, 1.0f, 0.2f});
        auto matBlue   = device.createMaterial(shader); matBlue->setParameter("uColor", std::array<float,3>{0.2f, 0.2f, 1.0f});
        auto matYellow = device.createMaterial(shader); matYellow->setParameter("uColor", std::array<float,3>{1.0f, 1.0f, 0.2f});
        CHECK_GL();

        // --- Инициализация физических тел ---
        std::vector<RigidBody> bodies(4);
        bodies[0].position = {3.0f, 0.0f, 0.0f};
        bodies[1].position = {0.0f, 3.0f, 0.0f};
        bodies[2].position = {0.0f, 0.0f, 3.0f};
        bodies[3].position = {2.0f, 2.0f, 2.0f};

        const float dt = 1.0f / 60.0f;
        const float restitution = 0.8f;
        const float attractionStrength = 2.0f;

        std::array<std::array<float,3>, 4> axes = {{{1,0,0}, {0,1,0}, {0,0,1}, {1,1,0}}};
        float angleSpeeds[4] = {1.2f, 1.5f, 1.8f, 2.0f};

        // Камера с управлением
        FlyCamera flyCam;
        flyCam.position = {8.0f, 6.0f, 12.0f};
        flyCam.yaw = -135.0f;
        flyCam.pitch = -20.0f;
        auto camera = std::make_shared<render::Camera>();
        
        // Переменные для мыши
        double lastMouseX = 1920.0 / 2.0;
        double lastMouseY = 1080.0 / 2.0;
        bool firstMouse = true;
        
        glfwSetCursorPosCallback(window, [](GLFWwindow* win, double xpos, double ypos) {
            // callback будет установлен ниже, но для простоты обработаем в цикле
        });

        auto lastTime = std::chrono::high_resolution_clock::now();
        float timeAcc = 0.0f;
        int frameCount = 0;

        std::cout << "Entering main loop with physics and free camera..." << std::endl;
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            // Обработка мыши (ручной захват позиции)
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            if (firstMouse) {
                lastMouseX = mouseX;
                lastMouseY = mouseY;
                firstMouse = false;
            }
            double xoffset = mouseX - lastMouseX;
            double yoffset = lastMouseY - mouseY; // инвертировано
            lastMouseX = mouseX;
            lastMouseY = mouseY;
            
            flyCam.processMouseMovement(xoffset, yoffset);

            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;
            timeAcc += deltaTime;
            
            // Управление клавиатурой
            flyCam.processKeyboard(window, deltaTime);
            
            // Применяем камеру
            float aspect = 1920.0f / 1080.0f;
            flyCam.applyToRenderer(camera, aspect);

            try {
                // Целевые позиции (орбиты)
                std::array<std::array<float,3>, 4> targets = {
                    std::array<float,3>{0.0f, 4.0f * std::cos(timeAcc * 0.7f), 4.0f * std::sin(timeAcc * 0.7f)},
                    std::array<float,3>{4.0f * std::cos(timeAcc * 0.5f), 0.0f, 4.0f * std::sin(timeAcc * 0.5f)},
                    std::array<float,3>{4.0f * std::cos(timeAcc * 0.4f), 4.0f * std::sin(timeAcc * 0.4f), 0.0f},
                    std::array<float,3>{5.0f * std::cos(timeAcc * 0.3f), 2.0f * std::sin(timeAcc * 0.8f) + 2.0f, 5.0f * std::sin(timeAcc * 0.3f)}
                };

                // Притяжение к орбитам
                for (int i = 0; i < 4; ++i) {
                    std::array<float,3> diff = {targets[i][0] - bodies[i].position[0],
                                               targets[i][1] - bodies[i].position[1],
                                               targets[i][2] - bodies[i].position[2]};
                    float len = std::sqrt(diff[0]*diff[0] + diff[1]*diff[1] + diff[2]*diff[2]);
                    if (len > 0.01f) {
                        diff[0] /= len; diff[1] /= len; diff[2] /= len;
                        bodies[i].velocity[0] += diff[0] * attractionStrength * deltaTime;
                        bodies[i].velocity[1] += diff[1] * attractionStrength * deltaTime;
                        bodies[i].velocity[2] += diff[2] * attractionStrength * deltaTime;
                    }
                }

                // Интеграция скоростей
                for (auto& body : bodies) {
                    body.position[0] += body.velocity[0] * deltaTime;
                    body.position[1] += body.velocity[1] * deltaTime;
                    body.position[2] += body.velocity[2] * deltaTime;
                }

                // Обработка столкновений
                const int iterations = 3;
                for (int iter = 0; iter < iterations; ++iter) {
                    for (size_t i = 0; i < bodies.size(); ++i) {
                        for (size_t j = i + 1; j < bodies.size(); ++j) {
                            auto& a = bodies[i];
                            auto& b = bodies[j];
                            
                            std::array<float,3> delta = {a.position[0] - b.position[0],
                                                        a.position[1] - b.position[1],
                                                        a.position[2] - b.position[2]};
                            float dist = std::sqrt(delta[0]*delta[0] + delta[1]*delta[1] + delta[2]*delta[2]);
                            float minDist = a.radius + b.radius;
                            
                            if (dist < minDist && dist > 1e-6f) {
                                std::array<float,3> n = {delta[0]/dist, delta[1]/dist, delta[2]/dist};
                                
                                float overlap = minDist - dist;
                                float totalMass = a.mass + b.mass;
                                float moveA = (b.mass / totalMass) * overlap;
                                float moveB = (a.mass / totalMass) * overlap;
                                a.position[0] += n[0] * moveA;
                                a.position[1] += n[1] * moveA;
                                a.position[2] += n[2] * moveA;
                                b.position[0] -= n[0] * moveB;
                                b.position[1] -= n[1] * moveB;
                                b.position[2] -= n[2] * moveB;
                                
                                std::array<float,3> vRel = {a.velocity[0] - b.velocity[0],
                                                           a.velocity[1] - b.velocity[1],
                                                           a.velocity[2] - b.velocity[2]};
                                float velAlongNormal = vRel[0]*n[0] + vRel[1]*n[1] + vRel[2]*n[2];
                                
                                if (velAlongNormal < 0) {
                                    float e = restitution;
                                    float j = -(1.0f + e) * velAlongNormal;
                                    j /= (1.0f/a.mass + 1.0f/b.mass);
                                    
                                    std::array<float,3> impulse = {j * n[0], j * n[1], j * n[2]};
                                    a.velocity[0] += impulse[0] / a.mass;
                                    a.velocity[1] += impulse[1] / a.mass;
                                    a.velocity[2] += impulse[2] / a.mass;
                                    b.velocity[0] -= impulse[0] / b.mass;
                                    b.velocity[1] -= impulse[1] / b.mass;
                                    b.velocity[2] -= impulse[2] / b.mass;
                                }
                            }
                        }
                    }
                }

                // Вычисление моторов и матриц
                std::array<Versor<PGA3D>, 4> motors;
                std::array<std::array<float,16>, 4> matrices;
                for (int i = 0; i < 4; ++i) {
                    motors[i] = PGA3D_impl::motor_from_axis_angle(axes[i], timeAcc * angleSpeeds[i], bodies[i].position);
                    matrices[i] = PGA3D_impl::motor_to_matrix(motors[i]);
                }

                // Построение сцены
                auto scene = std::make_shared<render::Scene>();
                scene->addRenderable(mesh, matRed,    matrices[0]);
                scene->addRenderable(mesh, matGreen,  matrices[1]);
                scene->addRenderable(mesh, matBlue,   matrices[2]);
                scene->addRenderable(mesh, matYellow, matrices[3]);

                // Рендеринг
                device.clear(0.1f, 0.1f, 0.15f, 1.0f);
                device.setViewport(0, 0, 1920, 1080);
                CHECK_GL();

                render::CommandBuffer cmd;
                cmd.begin();
                cmd.setRenderTarget(nullptr);
                cmd.clear(0.1f, 0.1f, 0.15f, 1.0f);
                cmd.setViewport(0, 0, 1920, 1080);
                cmd.drawScene(*scene, *camera);
                cmd.end();
                cmd.execute();
                CHECK_GL();

                device.swapBuffers();

                if (frameCount % 60 == 0) {
                    std::cout << std::fixed << std::setprecision(2);
                    std::cout << "Frame " << frameCount << " positions:\n";
                    std::cout << "  Red:   (" << bodies[0].position[0] << ", " << bodies[0].position[1] << ", " << bodies[0].position[2] << ")\n";
                    std::cout << "  Green: (" << bodies[1].position[0] << ", " << bodies[1].position[1] << ", " << bodies[1].position[2] << ")\n";
                    std::cout << "  Blue:  (" << bodies[2].position[0] << ", " << bodies[2].position[1] << ", " << bodies[2].position[2] << ")\n";
                    std::cout << "  Yellow:(" << bodies[3].position[0] << ", " << bodies[3].position[1] << ", " << bodies[3].position[2] << ")\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "Exception in frame " << frameCount << ": " << e.what() << std::endl;
                break;
            } catch (...) {
                std::cerr << "Unknown exception in frame " << frameCount << std::endl;
                break;
            }
            ++frameCount;
        }

        std::cout << "Exiting cleanly." << std::endl;
        device.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}