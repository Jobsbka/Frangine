// src/examples/doodlejump/DoodleJumpGame.cpp
#define GLFW_INCLUDE_NONE
#include "DoodleJumpGame.hpp"
#include "../../render/graphics_device.hpp"
#include "../../render/mesh.hpp"
#include "../../render/material.hpp"
#include "../../render/shader.hpp"
#include "../../render/command_buffer.hpp"
#include "../../nodes/uin/ui_render_node.hpp"
#include "../../nodes/node_factory.hpp"
#include "../../ui/input_manager.hpp"
#include "miniaudio.h"
#include <glad/glad.h>
#include <iostream>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <unordered_set>
#include <cstring>

namespace arxglue::game {

DoodleJumpGame::DoodleJumpGame() : Game("3Tone - Doodle Jump", 600, 800) {
    std::random_device rd;
    m_rng.seed(rd());
    m_uiContext = std::make_unique<Context>();
}

DoodleJumpGame::~DoodleJumpGame() {
    stopCurrentSound();
}

void DoodleJumpGame::stopCurrentSound() {
    if (m_currentSound) {
        ma_sound_stop(m_currentSound);
        ma_sound_uninit(m_currentSound);
        delete m_currentSound;
        m_currentSound = nullptr;
    }
    if (m_currentWaveform) {
        ma_waveform_uninit(m_currentWaveform);
        delete m_currentWaveform;
        m_currentWaveform = nullptr;
    }
}

void DoodleJumpGame::onInit() {
    std::cout << "[DoodleJump] onInit start" << std::endl;

    ma_result res = ma_engine_init(NULL, &m_audioEngine);
    if (res != MA_SUCCESS) {
        std::cerr << "[Sound] Failed to initialize audio engine: " << res << std::endl;
    } else {
        std::cout << "[Sound] Audio engine initialized" << std::endl;
        ma_engine_set_volume(&m_audioEngine, 1.0f);   // <-- ДОБАВЛЕНО: устанавливаем максимальную громкость
    }

    auto& device = render::GraphicsDevice::instance();
    std::vector<float> quadVerts = {
        0.0f,0.0f,0.0f, 0.0f,0.0f,1.0f, 0.0f,1.0f,
        1.0f,0.0f,0.0f, 0.0f,0.0f,1.0f, 1.0f,1.0f,
        1.0f,1.0f,0.0f, 0.0f,0.0f,1.0f, 1.0f,0.0f,
        0.0f,1.0f,0.0f, 0.0f,0.0f,1.0f, 0.0f,1.0f
    };
    std::vector<uint32_t> quadIdx = {0,1,2, 2,3,0};
    std::vector<int> attribSizes = {3,3,2};
    m_quadMesh = device.createMesh(quadVerts, quadIdx, attribSizes);

    std::vector<uint8_t> blue(4);
    blue[0] = 0; blue[1] = 0; blue[2] = 255; blue[3] = 255;
    m_playerTex = device.createTexture(1,1,4, blue.data());

    std::vector<uint8_t> lightBlue(4);
    lightBlue[0] = 0; lightBlue[1] = 200; lightBlue[2] = 255; lightBlue[3] = 255;
    m_normalPlatformTex = device.createTexture(1,1,4, lightBlue.data());

    std::vector<uint8_t> red(4);
    red[0] = 255; red[1] = 0; red[2] = 0; red[3] = 255;
    m_oneTimePlatformTex = device.createTexture(1,1,4, red.data());

    std::vector<uint8_t> green(4);
    green[0] = 0; green[1] = 255; green[2] = 0; green[3] = 255;
    m_highJumpPlatformTex = device.createTexture(1,1,4, green.data());

    auto shader = device.createShader(
        R"(#version 450 core
        layout(location=0) in vec3 aPos;
        layout(location=2) in vec2 aUV;
        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProjection;
        out vec2 vUV;
        void main() {
            vUV = aUV;
            gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
        })",
        R"(#version 450 core
        in vec2 vUV;
        uniform sampler2D uTexture;
        out vec4 FragColor;
        void main() {
            FragColor = texture(uTexture, vUV);
        })"
    );
    m_gameMaterial = device.createMaterial(shader);

    buildFullUIGraph();
    resetGame();
    std::cout << "[DoodleJump] onInit done" << std::endl;
}

void DoodleJumpGame::onShutdown() {
    stopCurrentSound();
    ma_engine_uninit(&m_audioEngine);
}

void DoodleJumpGame::buildFullUIGraph() {
    using namespace ui;

    m_uiGraph = Graph();
    m_uiContext = std::make_unique<Context>();
    m_uiExecutor = std::make_unique<Executor>(0);

    auto canvas = std::make_unique<CanvasNode>();
    canvas->setParameter("width", m_width);
    canvas->setParameter("height", m_height);
    m_uiGraph.addNode(std::move(canvas));

    // Главное меню (mode 0)
    auto titleMain = std::make_unique<TextNode>();
    titleMain->setParameter("position", std::array<float,2>{m_width/2.0f - 150.0f, 100.0f});
    titleMain->setParameter("text", std::string("3Tone Doodle Jump"));
    titleMain->setParameter("fontSize", 48.0f);
    titleMain->setParameter("color", std::array<float,3>{1.0f,1.0f,1.0f});
    titleMain->setParameter("zOrder", 10);
    titleMain->setParameter("visibleMode", 0);
    m_uiGraph.addNode(std::move(titleMain));

    auto playBtn = std::make_unique<ButtonNode>();
    playBtn->setParameter("id", std::string("play"));
    playBtn->setParameter("position", std::array<float,2>{m_width/2.0f - 60.0f, 300.0f});
    playBtn->setParameter("size", std::array<float,2>{120.0f, 50.0f});
    playBtn->setParameter("text", std::string("Play"));
    playBtn->setParameter("normalColor", std::array<float,4>{0.3f,0.6f,0.9f,1.0f});
    playBtn->setParameter("hoverColor", std::array<float,4>{0.4f,0.8f,1.0f,1.0f});
    playBtn->setParameter("pressedColor", std::array<float,4>{0.2f,0.4f,0.7f,1.0f});
    playBtn->setParameter("zOrder", 10);
    playBtn->setParameter("visibleMode", 0);
    m_uiGraph.addNode(std::move(playBtn));

    auto quitBtn = std::make_unique<ButtonNode>();
    quitBtn->setParameter("id", std::string("quit"));
    quitBtn->setParameter("position", std::array<float,2>{m_width/2.0f - 60.0f, 380.0f});
    quitBtn->setParameter("size", std::array<float,2>{120.0f, 50.0f});
    quitBtn->setParameter("text", std::string("Quit"));
    quitBtn->setParameter("normalColor", std::array<float,4>{0.7f,0.2f,0.2f,1.0f});
    quitBtn->setParameter("hoverColor", std::array<float,4>{1.0f,0.3f,0.3f,1.0f});
    quitBtn->setParameter("pressedColor", std::array<float,4>{0.5f,0.1f,0.1f,1.0f});
    quitBtn->setParameter("zOrder", 10);
    quitBtn->setParameter("visibleMode", 0);
    m_uiGraph.addNode(std::move(quitBtn));

    // Меню паузы (mode 1)
    auto titlePause = std::make_unique<TextNode>();
    titlePause->setParameter("position", std::array<float,2>{m_width/2.0f - 60.0f, 150.0f});
    titlePause->setParameter("text", std::string("Paused"));
    titlePause->setParameter("fontSize", 40.0f);
    titlePause->setParameter("color", std::array<float,3>{1.0f,1.0f,1.0f});
    titlePause->setParameter("zOrder", 10);
    titlePause->setParameter("visibleMode", 1);
    m_uiGraph.addNode(std::move(titlePause));

    auto resumeBtn = std::make_unique<ButtonNode>();
    resumeBtn->setParameter("id", std::string("resume"));
    resumeBtn->setParameter("position", std::array<float,2>{m_width/2.0f - 60.0f, 300.0f});
    resumeBtn->setParameter("size", std::array<float,2>{120.0f, 50.0f});
    resumeBtn->setParameter("text", std::string("Resume"));
    resumeBtn->setParameter("normalColor", std::array<float,4>{0.3f,0.6f,0.9f,1.0f});
    resumeBtn->setParameter("hoverColor", std::array<float,4>{0.4f,0.8f,1.0f,1.0f});
    resumeBtn->setParameter("pressedColor", std::array<float,4>{0.2f,0.4f,0.7f,1.0f});
    resumeBtn->setParameter("zOrder", 10);
    resumeBtn->setParameter("visibleMode", 1);
    m_uiGraph.addNode(std::move(resumeBtn));

    auto menuFromPause = std::make_unique<ButtonNode>();
    menuFromPause->setParameter("id", std::string("mainmenu_from_pause"));
    menuFromPause->setParameter("position", std::array<float,2>{m_width/2.0f - 60.0f, 380.0f});
    menuFromPause->setParameter("size", std::array<float,2>{120.0f, 50.0f});
    menuFromPause->setParameter("text", std::string("Main Menu"));
    menuFromPause->setParameter("normalColor", std::array<float,4>{0.7f,0.7f,0.7f,1.0f});
    menuFromPause->setParameter("hoverColor", std::array<float,4>{0.9f,0.9f,0.9f,1.0f});
    menuFromPause->setParameter("pressedColor", std::array<float,4>{0.5f,0.5f,0.5f,1.0f});
    menuFromPause->setParameter("zOrder", 10);
    menuFromPause->setParameter("visibleMode", 1);
    m_uiGraph.addNode(std::move(menuFromPause));

    // Game Over (mode 2)
    auto titleGO = std::make_unique<TextNode>();
    titleGO->setParameter("position", std::array<float,2>{m_width/2.0f - 100.0f, 150.0f});
    titleGO->setParameter("text", std::string("Game Over"));
    titleGO->setParameter("fontSize", 48.0f);
    titleGO->setParameter("color", std::array<float,3>{1.0f,0.2f,0.2f});
    titleGO->setParameter("zOrder", 10);
    titleGO->setParameter("visibleMode", 2);
    m_uiGraph.addNode(std::move(titleGO));

    auto scoreText = std::make_unique<TextNode>();
    scoreText->setParameter("id", std::string("scoreLabel"));
    scoreText->setParameter("position", std::array<float,2>{m_width/2.0f - 50.0f, 250.0f});
    scoreText->setParameter("text", std::string("Score: 0"));
    scoreText->setParameter("fontSize", 32.0f);
    scoreText->setParameter("color", std::array<float,3>{1.0f,1.0f,1.0f});
    scoreText->setParameter("zOrder", 10);
    scoreText->setParameter("visibleMode", 2);
    m_uiGraph.addNode(std::move(scoreText));

    auto retryBtn = std::make_unique<ButtonNode>();
    retryBtn->setParameter("id", std::string("retry"));
    retryBtn->setParameter("position", std::array<float,2>{m_width/2.0f - 60.0f, 350.0f});
    retryBtn->setParameter("size", std::array<float,2>{120.0f, 50.0f});
    retryBtn->setParameter("text", std::string("Retry"));
    retryBtn->setParameter("normalColor", std::array<float,4>{0.3f,0.6f,0.9f,1.0f});
    retryBtn->setParameter("hoverColor", std::array<float,4>{0.4f,0.8f,1.0f,1.0f});
    retryBtn->setParameter("pressedColor", std::array<float,4>{0.2f,0.4f,0.7f,1.0f});
    retryBtn->setParameter("zOrder", 10);
    retryBtn->setParameter("visibleMode", 2);
    m_uiGraph.addNode(std::move(retryBtn));

    auto menuFromGO = std::make_unique<ButtonNode>();
    menuFromGO->setParameter("id", std::string("mainmenu_from_gameover"));
    menuFromGO->setParameter("position", std::array<float,2>{m_width/2.0f - 60.0f, 430.0f});
    menuFromGO->setParameter("size", std::array<float,2>{120.0f, 50.0f});
    menuFromGO->setParameter("text", std::string("Main Menu"));
    menuFromGO->setParameter("normalColor", std::array<float,4>{0.7f,0.7f,0.7f,1.0f});
    menuFromGO->setParameter("hoverColor", std::array<float,4>{0.9f,0.9f,0.9f,1.0f});
    menuFromGO->setParameter("pressedColor", std::array<float,4>{0.5f,0.5f,0.5f,1.0f});
    menuFromGO->setParameter("zOrder", 10);
    menuFromGO->setParameter("visibleMode", 2);
    m_uiGraph.addNode(std::move(menuFromGO));

    // Игровые виджеты (mode 3)
    auto heightText = std::make_unique<TextNode>();
    heightText->setParameter("id", std::string("heightWidget"));
    heightText->setParameter("position", std::array<float,2>{10.0f, 10.0f});
    heightText->setParameter("text", std::string("Height: 0"));
    heightText->setParameter("fontSize", 24.0f);
    heightText->setParameter("color", std::array<float,3>{1.0f,1.0f,1.0f});
    heightText->setParameter("zOrder", 20);
    heightText->setParameter("visibleMode", 3);
    m_uiGraph.addNode(std::move(heightText));

    auto recordText = std::make_unique<TextNode>();
    recordText->setParameter("id", std::string("recordWidget"));
    recordText->setParameter("position", std::array<float,2>{m_width - 150.0f, 10.0f});
    recordText->setParameter("text", std::string("Record: 0"));
    recordText->setParameter("fontSize", 24.0f);
    recordText->setParameter("color", std::array<float,3>{1.0f,1.0f,1.0f});
    recordText->setParameter("zOrder", 20);
    recordText->setParameter("visibleMode", 3);
    m_uiGraph.addNode(std::move(recordText));

    auto renderNode = std::make_unique<ui::UIRenderNode>();
    m_uiGraph.addNode(std::move(renderNode));
}

void DoodleJumpGame::resetGame() {
    m_player = { m_width/2.0f, 100.0f, 0.0f, 0.0f, 30.0f, 30.0f };
    m_cameraY = 0.0f;
    m_score = 0;
    m_gameOver = false;
    m_platforms.clear();
    generateInitialPlatforms();
    m_prevPlayerY = m_player.y;
    m_maxHeight = m_player.y;
    std::cout << "[DoodleJump] Game reset. Player at (" << m_player.x << "," << m_player.y 
              << "), platforms: " << m_platforms.size() << std::endl;
}

void DoodleJumpGame::generateInitialPlatforms() {
    std::uniform_real_distribution<float> xDist(50.0f, m_width - 130.0f);
    std::uniform_int_distribution<int> typeDist(0, 9);
    const float minVerticalGap = 150.0f;
    const float maxVerticalStep = 220.0f;
    const float minHorizontalGap = 100.0f;

    float yPos = 50.0f;
    m_platforms.push_back({m_player.x - 40.0f, yPos, 80.0f, 15.0f, Platform::Normal});
    yPos += minVerticalGap;

    for (int i = 0; i < 10; ++i) {
        Platform::Type t = Platform::Normal;
        int r = typeDist(m_rng);
        if (r < 7) t = Platform::Normal;
        else if (r < 8) t = Platform::OneTime;
        else t = Platform::HighJump;

        float x = xDist(m_rng);
        bool tooClose;
        int attempts = 0;
        do {
            tooClose = false;
            for (const auto& p : m_platforms) {
                float dx = std::abs(x - p.x);
                float dy = std::abs(yPos - p.y);
                if (dx < minHorizontalGap && dy < minVerticalGap) {
                    tooClose = true;
                    x = xDist(m_rng);
                    break;
                }
            }
            if (++attempts > 30) break;
        } while (tooClose);

        m_platforms.push_back({x, yPos, 80.0f, 15.0f, t});
        yPos += std::uniform_real_distribution<float>(minVerticalGap, maxVerticalStep)(m_rng);
    }
}

void DoodleJumpGame::onUpdate(float deltaTime) {
    ui::InputManager::instance().update(m_window);
    ui::InputManager::instance().writeToContext(*m_uiContext);

    if (m_state == GameState::MainMenu) {
        m_uiContext->setState("ui.mode", 0);
    } else if (m_state == GameState::Paused) {
        m_uiContext->setState("ui.mode", 1);
    } else if (m_gameOver) {
        m_uiContext->setState("ui.mode", 2);
    } else {
        m_uiContext->setState("ui.mode", 3);
    }

    if (m_uiContext->hasState("ui.button.play.clicked") && m_uiContext->getState<bool>("ui.button.play.clicked")) {
        std::cout << "[DoodleJump] Play clicked, switching to Playing" << std::endl;
        m_state = GameState::Playing;
        m_gameOver = false;
        resetGame();
    }
    if (m_uiContext->hasState("ui.button.quit.clicked") && m_uiContext->getState<bool>("ui.button.quit.clicked")) {
        std::cout << "[DoodleJump] Quit clicked, closing window" << std::endl;
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }
    if (m_uiContext->hasState("ui.button.resume.clicked") && m_uiContext->getState<bool>("ui.button.resume.clicked")) {
        m_state = GameState::Playing;
    }
    if (m_uiContext->hasState("ui.button.mainmenu_from_pause.clicked") && m_uiContext->getState<bool>("ui.button.mainmenu_from_pause.clicked")) {
        m_state = GameState::MainMenu;
        m_gameOver = false;
    }
    if (m_uiContext->hasState("ui.button.retry.clicked") && m_uiContext->getState<bool>("ui.button.retry.clicked")) {
        m_state = GameState::Playing;
        m_gameOver = false;
        resetGame();
    }
    if (m_uiContext->hasState("ui.button.mainmenu_from_gameover.clicked") && m_uiContext->getState<bool>("ui.button.mainmenu_from_gameover.clicked")) {
        m_state = GameState::MainMenu;
        m_gameOver = false;
    }

    m_uiContext->setState("ui.button.play.clicked", false);
    m_uiContext->setState("ui.button.quit.clicked", false);
    m_uiContext->setState("ui.button.resume.clicked", false);
    m_uiContext->setState("ui.button.mainmenu_from_pause.clicked", false);
    m_uiContext->setState("ui.button.retry.clicked", false);
    m_uiContext->setState("ui.button.mainmenu_from_gameover.clicked", false);

    if (m_state == GameState::Playing && !m_gameOver) {
        m_prevPlayerY = m_player.y;

        const float gravity = 800.0f;
        const float moveSpeed = 400.0f;
        m_player.vy -= gravity * deltaTime;
        if (m_leftPressed) m_player.vx = -moveSpeed;
        else if (m_rightPressed) m_player.vx = moveSpeed;
        else m_player.vx = 0.0f;

        m_player.x += m_player.vx * deltaTime;
        m_player.y += m_player.vy * deltaTime;

        if (m_player.x < 0.0f) m_player.x = 0.0f;
        if (m_player.x > m_width - m_player.width) m_player.x = m_width - m_player.width;

        checkCollisionsAndJump();

        float screenMidY = m_cameraY + m_height/2.0f;
        if (m_player.y > screenMidY) {
            m_cameraY = m_player.y - m_height/2.0f;
        }

        updatePlatforms(deltaTime);

        if (m_player.y > m_maxHeight) {
            m_maxHeight = m_player.y;
        }

        std::ostringstream oss;
        int heightDisplay = static_cast<int>(m_player.y / 100.0f);
        oss << "Height: " << heightDisplay;
        m_uiContext->setState("ui.text.heightWidget", oss.str());
        oss.str("");
        int recordDisplay = static_cast<int>(m_maxHeight / 100.0f);
        oss << "Record: " << recordDisplay;
        m_uiContext->setState("ui.text.recordWidget", oss.str());

        if (m_player.y < m_cameraY - 50.0f) {
            m_gameOver = true;
            m_state = GameState::MainMenu;
        }
    }

    // Проверяем, не закончился ли звук
    if (m_currentSound && !ma_sound_is_playing(m_currentSound)) {
        stopCurrentSound();
    }
}

void DoodleJumpGame::checkCollisionsAndJump() {
    if (m_player.vy <= 0.0f) {
        for (auto it = m_platforms.begin(); it != m_platforms.end(); ) {
            auto& p = *it;
            bool overlapX = m_player.x + m_player.width > p.x && m_player.x < p.x + p.w;
            bool overlapY = m_player.y + m_player.height > p.y && m_player.y < p.y + p.h;
            bool wasAbove = m_prevPlayerY >= p.y + p.h;

            if (overlapX && overlapY && wasAbove) {
                float jumpForce = 700.0f;
                if (p.type == Platform::HighJump) {
                    jumpForce = 1000.0f;
                }
                m_player.vy = jumpForce;
                m_player.y = p.y + p.h;
                m_score += 10;

                // Останавливаем предыдущий звук
                stopCurrentSound();

                // Создаём конфигурацию waveform (вручную)
                ma_waveform_config waveformConfig;
                memset(&waveformConfig, 0, sizeof(waveformConfig));
                waveformConfig.format   = ma_format_f32;
                waveformConfig.channels = 1;
                waveformConfig.type     = ma_waveform_type_sine;
                waveformConfig.frequency = 440.0f;
                waveformConfig.amplitude = 0.5f;

                if (p.type == Platform::HighJump) {
                    waveformConfig.frequency = 880.0f;
                } else if (p.type == Platform::OneTime) {
                    waveformConfig.type = ma_waveform_type_square;
                    waveformConfig.frequency = 60.0f;
                    waveformConfig.amplitude = 0.3f;
                }

                // Создаём waveform и sound
                ma_waveform* pWaveform = new ma_waveform;
                ma_sound* pSound = new ma_sound;

                if (ma_waveform_init(&waveformConfig, pWaveform) == MA_SUCCESS) {
                    if (ma_sound_init_from_data_source(&m_audioEngine, pWaveform, 0, NULL, pSound) == MA_SUCCESS) {
                        ma_sound_set_volume(pSound, 1.0f);
                        ma_sound_start(pSound);
                        ma_sound_set_stop_time_in_milliseconds(pSound, 200);
                        m_currentSound = pSound;
                        m_currentWaveform = pWaveform;
                        std::cout << "[Sound] Played sound for type " << (int)p.type << std::endl;
                    } else {
                        ma_waveform_uninit(pWaveform);
                        delete pWaveform;
                        delete pSound;
                    }
                } else {
                    delete pWaveform;
                    delete pSound;
                }

                if (p.type == Platform::OneTime) {
                    it = m_platforms.erase(it);
                } else {
                    ++it;
                }
                break;
            } else {
                ++it;
            }
        }
    }
}

void DoodleJumpGame::updatePlatforms(float deltaTime) {
    (void)deltaTime;
    m_platforms.erase(std::remove_if(m_platforms.begin(), m_platforms.end(),
        [this](const Platform& p) { return p.y + p.h < m_cameraY - 100.0f; }), m_platforms.end());

    const float minVerticalGap = 150.0f;
    const float maxVerticalStep = 220.0f;
    const float minHorizontalGap = 100.0f;

    float highestY = m_cameraY + m_height + 100.0f;
    if (!m_platforms.empty()) {
        float maxY = m_platforms[0].y;
        for (const auto& p : m_platforms) {
            if (p.y > maxY) maxY = p.y;
        }
        if (maxY > highestY) highestY = maxY + minVerticalGap;
    }

    std::uniform_real_distribution<float> xDist(50.0f, m_width - 130.0f);
    std::uniform_int_distribution<int> typeDist(0, 9);

    while (m_platforms.size() < 15) {
        Platform::Type t = Platform::Normal;
        int r = typeDist(m_rng);
        if (r < 7) t = Platform::Normal;
        else if (r < 8) t = Platform::OneTime;
        else t = Platform::HighJump;

        float x = xDist(m_rng);
        bool tooClose;
        int attempts = 0;
        do {
            tooClose = false;
            for (const auto& p : m_platforms) {
                float dx = std::abs(x - p.x);
                float dy = std::abs(highestY - p.y);
                if (dx < minHorizontalGap && dy < minVerticalGap) {
                    tooClose = true;
                    x = xDist(m_rng);
                    break;
                }
            }
            if (++attempts > 30) break;
        } while (tooClose);

        m_platforms.push_back({x, highestY, 80.0f, 15.0f, t});
        highestY += std::uniform_real_distribution<float>(minVerticalGap, maxVerticalStep)(m_rng);
    }
}

void DoodleJumpGame::onRender() {
    auto& device = render::GraphicsDevice::instance();
    device.makeCurrent();

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (m_state == GameState::Playing) {
        device.clear(0.2f, 0.3f, 0.3f, 1.0f);
    } else {
        device.clear(0.1f, 0.1f, 0.15f, 1.0f);
    }

    if (m_state == GameState::Playing || m_gameOver) {
        drawGameObjects();
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, m_width, m_height);

    try {
        m_uiExecutor->execute(m_uiGraph, *m_uiContext);
    } catch (const std::exception& e) {
        std::cerr << "[DoodleJump] UI executor exception: " << e.what() << std::endl;
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "[DoodleJump] OpenGL error after frame: " << err << std::endl;
    }
}

void DoodleJumpGame::drawGameObjects() {
    if (!m_gameMaterial) return;
    if (!m_quadMesh) return;

    static int frameCount = 0;
    if (frameCount++ % 60 == 0) {
        std::cout << "[DoodleJump] Drawing " << m_platforms.size() << " platforms, player at (" 
                  << m_player.x << "," << m_player.y << ")" << std::endl;
    }

    render::CommandBuffer cmd;
    cmd.begin();
    cmd.setRenderTarget(nullptr);
    cmd.setViewport(0, 0, m_width, m_height);

    std::array<float,16> proj = {
        2.0f / m_width, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / m_height, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f
    };
    std::array<float,16> view = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, -m_cameraY, 0.0f, 1.0f
    };

    m_gameMaterial->setParameter("uView", view);
    m_gameMaterial->setParameter("uProjection", proj);

    for (auto& p : m_platforms) {
        std::shared_ptr<render::Texture> tex;
        switch (p.type) {
            case Platform::Normal:   tex = m_normalPlatformTex; break;
            case Platform::OneTime:  tex = m_oneTimePlatformTex; break;
            case Platform::HighJump: tex = m_highJumpPlatformTex; break;
        }
        std::array<float,16> model = {
            p.w, 0.0f, 0.0f, 0.0f,
            0.0f, p.h, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            p.x, p.y, 0.0f, 1.0f
        };
        cmd.drawMesh(m_quadMesh, m_gameMaterial, model, tex);
    }

    std::array<float,16> playerModel = {
        m_player.width, 0.0f, 0.0f, 0.0f,
        0.0f, m_player.height, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        m_player.x, m_player.y, 0.0f, 1.0f
    };
    cmd.drawMesh(m_quadMesh, m_gameMaterial, playerModel, m_playerTex);

    cmd.end();
    cmd.execute();
}

void DoodleJumpGame::onKey(int key, int scancode, int action, int mods) {
    (void)scancode; (void)mods;
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_A || key == GLFW_KEY_LEFT) m_leftPressed = true;
        if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT) m_rightPressed = true;
        if (key == GLFW_KEY_ESCAPE) {
            if (m_state == GameState::Playing) {
                m_state = GameState::Paused;
            } else if (m_state == GameState::Paused) {
                m_state = GameState::Playing;
            }
        }
    }
    if (action == GLFW_RELEASE) {
        if (key == GLFW_KEY_A || key == GLFW_KEY_LEFT) m_leftPressed = false;
        if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT) m_rightPressed = false;
    }
}

void DoodleJumpGame::onMouseButton(int button, int action, int mods) {
    (void)button; (void)action; (void)mods;
}

void DoodleJumpGame::onResize(int width, int height) {
    m_width = width;
    m_height = height;
}

} // namespace arxglue::game