#include "DoodleJumpGame.hpp"
#include "../render/graphics_device.hpp"
#include "../render/command_buffer.hpp"
#include "../render/scene.hpp"
#include "../render/camera.hpp"
#include <glad/glad.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <cstring>

#define _USE_MATH_DEFINES
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "miniaudio.h"

namespace arxglue::game {

static std::shared_ptr<render::Mesh> createCubeMesh(float size) {
    std::vector<float> vertices = {
        -size, -size,  size,  0,0,1,  size, -size,  size,  0,0,1,
         size,  size,  size,  0,0,1, -size,  size,  size,  0,0,1,
        -size, -size, -size,  0,0,-1, size, -size, -size,  0,0,-1,
         size,  size, -size,  0,0,-1,-size,  size, -size,  0,0,-1,
        -size, -size, -size, -1,0,0, -size, -size,  size, -1,0,0,
        -size,  size,  size, -1,0,0, -size,  size, -size, -1,0,0,
         size, -size, -size,  1,0,0,  size, -size,  size,  1,0,0,
         size,  size,  size,  1,0,0,  size,  size, -size,  1,0,0,
        -size,  size, -size,  0,1,0, -size,  size,  size,  0,1,0,
         size,  size,  size,  0,1,0,  size,  size, -size,  0,1,0,
        -size, -size, -size,  0,-1,0,-size, -size,  size,  0,-1,0,
         size, -size,  size,  0,-1,0, size, -size, -size,  0,-1,0
    };
    std::vector<uint32_t> indices = {
        0,1,2,2,3,0, 4,5,6,6,7,4, 8,9,10,10,11,8,
        12,13,14,14,15,12, 16,17,18,18,19,16, 20,21,22,22,23,20
    };
    std::vector<int> attribSizes = {3, 3};
    return render::GraphicsDevice::instance().createMesh(vertices, indices, attribSizes);
}

static std::shared_ptr<render::Mesh> createPlatformMesh(float width, float height, float depth) {
    float w2 = width * 0.5f, h2 = height * 0.5f, d2 = depth * 0.5f;
    std::vector<float> vertices = {
        -w2, -h2,  d2,  0,0,1,  w2, -h2,  d2,  0,0,1,  w2,  h2,  d2,  0,0,1, -w2,  h2,  d2,  0,0,1,
        -w2, -h2, -d2,  0,0,-1, w2, -h2, -d2,  0,0,-1, w2,  h2, -d2,  0,0,-1,-w2,  h2, -d2,  0,0,-1,
        -w2, -h2, -d2, -1,0,0, -w2, -h2,  d2, -1,0,0, -w2,  h2,  d2, -1,0,0, -w2,  h2, -d2, -1,0,0,
         w2, -h2, -d2,  1,0,0,  w2, -h2,  d2,  1,0,0,  w2,  h2,  d2,  1,0,0,  w2,  h2, -d2,  1,0,0,
        -w2,  h2, -d2,  0,1,0, -w2,  h2,  d2,  0,1,0,  w2,  h2,  d2,  0,1,0,  w2,  h2, -d2,  0,1,0,
        -w2, -h2, -d2,  0,-1,0,-w2, -h2,  d2,  0,-1,0, w2, -h2,  d2,  0,-1,0, w2, -h2, -d2,  0,-1,0
    };
    std::vector<uint32_t> indices = {
        0,1,2,2,3,0, 4,5,6,6,7,4, 8,9,10,10,11,8,
        12,13,14,14,15,12, 16,17,18,18,19,16, 20,21,22,22,23,20
    };
    std::vector<int> attribSizes = {3, 3};
    return render::GraphicsDevice::instance().createMesh(vertices, indices, attribSizes);
}

DoodleJumpGame::DoodleJumpGame()
    : Game("3Tone Doodle Jump", 800, 600)
    , m_rng(std::random_device{}())
    , m_engine{}
    , m_jumpWaveform{}
    , m_jumpSound{}
    , m_jumpSoundTimer(0.0f)
{
}

DoodleJumpGame::~DoodleJumpGame() {
    ma_sound_uninit(&m_jumpSound);
    ma_waveform_uninit(&m_jumpWaveform);
    ma_engine_uninit(&m_engine);
}

void DoodleJumpGame::onInit() {
    auto& device = render::GraphicsDevice::instance();

    m_playerMesh = createCubeMesh(0.4f);
    m_platformMesh = createPlatformMesh(1.2f, 0.2f, 0.8f);

    auto shader = device.createShader(
        R"(
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
        )",
        R"(
            #version 450 core
            in vec3 vNormal;
            in vec3 vFragPos;
            uniform vec3 uColor;
            uniform vec3 uLightDir;
            out vec4 FragColor;
            void main() {
                vec3 lightDir = normalize(uLightDir);
                vec3 normal = normalize(vNormal);
                float diff = max(dot(normal, lightDir), 0.0);
                vec3 ambient = vec3(0.2);
                vec3 result = (ambient + diff) * uColor;
                FragColor = vec4(result, 1.0);
            }
        )"
    );

    m_playerMaterial = device.createMaterial(shader);
    m_playerMaterial->setParameter("uColor", std::array<float,3>{0.2f, 0.6f, 1.0f});
    m_playerMaterial->setParameter("uLightDir", std::array<float,3>{1.0f, 2.0f, 1.0f});

    m_platformMaterial = device.createMaterial(shader);
    m_platformMaterial->setParameter("uLightDir", std::array<float,3>{1.0f, 2.0f, 1.0f});
    m_platformMaterial->setParameter("uColor", std::array<float,3>{0.7f, 0.5f, 0.3f});

    m_camera = std::make_shared<render::Camera>();

    for (int i = 0; i < 8; ++i) {
        Platform plat;
        float y = -2.0f + i * 1.2f;
        float x = (m_rng() % 100) / 100.0f * 4.0f - 2.0f;
        plat.position = {x, y, 0.0f};
        plat.scale = {1.2f, 0.2f, 0.8f};
        plat.isBouncy = (std::abs(y) < 0.1f);
        plat.isBreakable = (y > 3.0f);
        plat.color = plat.isBouncy ? std::array<float,3>{0.2f, 0.9f, 0.3f}
                   : plat.isBreakable ? std::array<float,3>{0.9f, 0.2f, 0.2f}
                   : std::array<float,3>{0.7f, 0.5f, 0.3f};
        m_platforms.push_back(plat);
    }

    // ------------------ АУДИО (ma_engine) ------------------
    ma_engine_config engineConfig = ma_engine_config_init();
    engineConfig.listenerCount = 1;
    engineConfig.channels = 2;
    engineConfig.sampleRate = 44100;

    ma_result result = ma_engine_init(&engineConfig, &m_engine);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to initialize audio engine. Error: " << result << std::endl;
        return;
    }
    std::cout << "[Audio] Engine initialized successfully." << std::endl;
    ma_engine_start(&m_engine);
    std::cout << "[Audio] Engine started." << std::endl;

    // Создаём waveform для звука прыжка
    ma_waveform_config waveConfig = ma_waveform_config_init(
        ma_format_f32,
        1,          // моно
        44100,
        ma_waveform_type_sine,
        1.0f,       // громкость 100%
        660.0f      // частота 660 Гц
    );
    ma_waveform_init(&waveConfig, &m_jumpWaveform);

    ma_sound_config soundConfig = ma_sound_config_init();
    soundConfig.pDataSource = &m_jumpWaveform;
    ma_sound_init_ex(&m_engine, &soundConfig, &m_jumpSound);
    ma_sound_set_volume(&m_jumpSound, 1.0f);
    ma_sound_set_looping(&m_jumpSound, MA_TRUE);  // включаем зацикливание, останавливать будем сами по таймеру
    ma_sound_stop(&m_jumpSound);

    std::cout << "[Audio] Jump sound ready (660 Hz, 1.0 amplitude)." << std::endl;
}

void DoodleJumpGame::onUpdate(float deltaTime) {
    updatePhysics(deltaTime);
    spawnNewPlatforms();

    // Управление длительностью звука прыжка
    if (m_jumpSoundTimer > 0.0f) {
        m_jumpSoundTimer -= deltaTime;
        if (m_jumpSoundTimer <= 0.0f) {
            ma_sound_stop(&m_jumpSound);
            std::cout << "[Audio] Jump sound stopped by timer." << std::endl;
        }
    }

    std::string title = "3Tone Doodle Jump | Height: " + std::to_string(static_cast<int>(m_score));
    glfwSetWindowTitle(m_window, title.c_str());
}

void DoodleJumpGame::onRender() {
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);
    float aspect = static_cast<float>(width) / height;

    m_camera->setPerspective(45.0f * static_cast<float>(M_PI) / 180.0f, aspect, 0.1f, 100.0f);
    m_camera->lookAt({0.0f, m_playerPos[1] + 2.0f, 8.0f}, {0.0f, m_playerPos[1], 0.0f}, {0.0f, 1.0f, 0.0f});

    render::Scene scene;

    std::array<float, 16> playerMat = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        m_playerPos[0], m_playerPos[1], m_playerPos[2], 1
    };
    scene.addRenderable(m_playerMesh, m_playerMaterial, playerMat);

    for (const auto& plat : m_platforms) {
        if (plat.scale[0] == 0.0f) continue;
        std::array<float, 16> mat = {
            plat.scale[0],0,0,0,
            0,plat.scale[1],0,0,
            0,0,plat.scale[2],0,
            plat.position[0], plat.position[1], plat.position[2], 1
        };
        auto matInst = render::GraphicsDevice::instance().createMaterial(m_platformMaterial->getShader());
        matInst->setParameter("uColor", plat.color);
        matInst->setParameter("uLightDir", std::array<float,3>{1.0f, 2.0f, 1.0f});
        scene.addRenderable(m_platformMesh, matInst, mat);
    }

    render::CommandBuffer cmd;
    cmd.begin();
    cmd.setRenderTarget(nullptr);
    cmd.clear(0.2f, 0.3f, 0.3f, 1.0f);
    cmd.setViewport(0, 0, width, height);
    cmd.drawScene(scene, *m_camera);
    cmd.end();
    cmd.execute();
}

void DoodleJumpGame::onKey(int key, int /*scancode*/, int action, int /*mods*/) {
    if (key == GLFW_KEY_A || key == GLFW_KEY_LEFT) {
        m_moveLeft = (action != GLFW_RELEASE);
    } else if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT) {
        m_moveRight = (action != GLFW_RELEASE);
    }
}

void DoodleJumpGame::updatePhysics(float dt) {
    const float gravity = -12.0f;
    const float playerHalfSize = 0.2f;
    const float speed = 5.0f;

    if (m_moveLeft) m_playerVel[0] = -speed;
    else if (m_moveRight) m_playerVel[0] = speed;
    else m_playerVel[0] *= 0.95f;

    m_playerVel[1] += gravity * dt;
    float newY = m_playerPos[1] + m_playerVel[1] * dt;

    float playerBottomOld = m_playerPos[1] - playerHalfSize;
    float playerBottomNew = newY - playerHalfSize;
    bool collided = false;
    float collisionY = 0.0f;
    Platform* collidedPlat = nullptr;

    for (auto& plat : m_platforms) {
        if (plat.scale[0] == 0.0f) continue;
        float platLeft = plat.position[0] - plat.scale[0] * 0.5f;
        float platRight = plat.position[0] + plat.scale[0] * 0.5f;
        float platTop = plat.position[1] + plat.scale[1] * 0.5f;

        bool xOverlap = (m_playerPos[0] + playerHalfSize > platLeft) &&
                        (m_playerPos[0] - playerHalfSize < platRight);
        if (xOverlap && m_playerVel[1] <= 0.0f && playerBottomNew <= platTop && playerBottomOld > platTop - 0.3f) {
            collided = true;
            collisionY = platTop + playerHalfSize;
            collidedPlat = &plat;
            break;
        }
    }

    if (collided && collidedPlat) {
        m_playerPos[1] = collisionY;
        m_playerVel[1] = collidedPlat->isBouncy ? 9.0f : 6.5f;
        if (collidedPlat->isBreakable) {
            collidedPlat->scale = {0,0,0};
        }
        // Воспроизводим звук прыжка
        if (ma_engine_get_device(&m_engine) != nullptr) {
            std::cout << "[Audio] Playing jump sound" << std::endl;
            ma_sound_stop(&m_jumpSound);           // останавливаем, если уже играет
            ma_sound_seek_to_pcm_frame(&m_jumpSound, 0);
            ma_result res = ma_sound_start(&m_jumpSound);
            if (res == MA_SUCCESS) {
                m_jumpSoundTimer = 0.5f;            // звук будет играть 500 мс
            } else {
                std::cerr << "[Audio] Failed to start jump sound: " << res << std::endl;
            }
        }
    } else {
        m_playerPos[1] = newY;
    }

    m_playerPos[0] += m_playerVel[0] * dt;
    const float boundX = 3.5f;
    m_playerPos[0] = (std::max)(-boundX, (std::min)(m_playerPos[0], boundX));

    m_score = (std::max)(m_score, m_playerPos[1] + 1.0f);

    if (m_playerPos[1] < -5.0f) {
        m_playerPos = {0,0,0};
        m_playerVel = {0,0,0};
        m_score = 0.0f;
        m_platforms.clear();
        for (int i = 0; i < 8; ++i) {
            Platform plat;
            float y = -2.0f + i * 1.2f;
            float x = (m_rng() % 100) / 100.0f * 4.0f - 2.0f;
            plat.position = {x, y, 0.0f};
            plat.scale = {1.2f, 0.2f, 0.8f};
            plat.isBouncy = (std::abs(y) < 0.1f);
            plat.isBreakable = (y > 3.0f);
            plat.color = plat.isBouncy ? std::array<float,3>{0.2f, 0.9f, 0.3f}
                       : plat.isBreakable ? std::array<float,3>{0.9f, 0.2f, 0.2f}
                       : std::array<float,3>{0.7f, 0.5f, 0.3f};
            m_platforms.push_back(plat);
        }
    }
}

void DoodleJumpGame::spawnNewPlatforms() {
    if (m_playerPos[1] <= 2.0f) return;

    float topY = -1000.0f;
    for (auto& p : m_platforms) {
        if (p.position[1] > topY) topY = p.position[1];
    }

    if (topY < m_playerPos[1] + 4.0f) {
        float newY = topY + 1.2f;
        float newX = (m_rng() % 100) / 100.0f * 5.0f - 2.5f;
        Platform newPlat;
        newPlat.position = {newX, newY, 0.0f};
        newPlat.scale = {1.2f, 0.2f, 0.8f};
        newPlat.isBouncy = (m_rng() % 10) == 0;
        newPlat.isBreakable = (newY > 5.0f) && ((m_rng() % 3) == 0);
        newPlat.color = newPlat.isBouncy ? std::array<float,3>{0.2f, 0.9f, 0.3f}
                      : newPlat.isBreakable ? std::array<float,3>{0.9f, 0.2f, 0.2f}
                      : std::array<float,3>{0.7f, 0.5f, 0.3f};
        m_platforms.push_back(newPlat);
    }

    m_platforms.erase(std::remove_if(m_platforms.begin(), m_platforms.end(),
        [this](const Platform& p) { return p.position[1] < m_playerPos[1] - 4.0f; }),
        m_platforms.end());
}

} // namespace arxglue::game