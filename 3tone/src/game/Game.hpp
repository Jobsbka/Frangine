// src/game/Game.hpp
#pragma once

#include <vector>
#include <memory>
#include <array>
#include <atomic>
#include <mutex>
#include <random>

#include "../render/graphics_device.hpp"
#include "../render/scene.hpp"
#include "../render/camera.hpp"
#include "../render/mesh.hpp"
#include "../render/material.hpp"
#include "../core/graph.hpp"
#include "../core/executor.hpp"

namespace arxglue::game {

struct Platform {
    std::array<float, 3> position;
    std::array<float, 3> scale;
    std::array<float, 3> color;
    bool isBouncy;
    bool isBreakable;
};

class Game {
public:
    Game(GLFWwindow* window);
    ~Game();

    void run();

private:
    void processInput(float dt);
    void updatePhysics(float dt);
    void generatePlatformsProcedurally(float playerY);
    void updateScene();
    void updateScoreTexture();
    void initProceduralColorGraph();
    std::array<float,3> computePlatformColor(float y, bool isBouncy, bool isBreakable);
    std::array<float,16> makeBillboardMatrix(const std::array<float,3>& position, float width, float height);

    GLFWwindow* m_window;
    render::Camera m_camera;
    std::shared_ptr<render::Mesh> m_playerMesh;
    std::shared_ptr<render::Mesh> m_platformMesh;
    std::shared_ptr<render::Material> m_playerMaterial;
    std::shared_ptr<render::Material> m_platformMaterialBase;
    std::shared_ptr<render::Material> m_scoreMaterial;
    std::shared_ptr<render::Mesh> m_quadMesh;

    // Игровое состояние
    std::array<float, 3> m_playerPos = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> m_playerVel = {0.0f, 0.0f, 0.0f};
    float m_score = 0.0f;
    float m_maxScore = 0.0f;

    std::vector<Platform> m_platforms;
    std::mutex m_platformsMutex;

    bool m_moveLeft = false;
    bool m_moveRight = false;

    // Процедурная генерация (теперь однопоточная)
    std::mt19937 m_rng;
    std::atomic<bool> m_genRequested{false};

    // Граф для цвета платформ (защищён мьютексом)
    std::unique_ptr<Graph> m_colorGraph;
    std::unique_ptr<Executor> m_colorExecutor;
    NodeId m_colorGraphYInputId;
    NodeId m_colorGraphOutputId;
    Context m_colorCtx;
    std::mutex m_colorMutex;

    // Граф для текста счёта
    std::unique_ptr<Graph> m_textGraph;
    std::unique_ptr<Executor> m_textExecutor;
    NodeId m_textGraphOutputId;
    Context m_textCtx;
    std::shared_ptr<render::Texture> m_scoreTexture;
    float m_lastScoreRendered = -1.0f;

    // Запечённая текстура
    std::shared_ptr<render::Texture> m_bakedCheckerTexture;
};

} // namespace arxglue::game