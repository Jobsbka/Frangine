// src/examples/doodlejump/DoodleJumpGame.hpp
#pragma once
#include "../../game/Game.hpp"
#include "../../core/graph.hpp"
#include "../../core/executor.hpp"
#include "../../core/context.hpp"
#include "../../nodes/uin/ui_nodes.hpp"
#include "miniaudio.h"
#include <memory>
#include <vector>
#include <random>

namespace arxglue::game {

class DoodleJumpGame : public Game {
public:
    DoodleJumpGame();
    ~DoodleJumpGame() override;

protected:
    void onInit() override;
    void onUpdate(float deltaTime) override;
    void onRender() override;
    void onShutdown() override;
    void onKey(int key, int scancode, int action, int mods) override;
    void onMouseButton(int button, int action, int mods) override;
    void onResize(int width, int height) override;

private:
    enum class GameState { MainMenu, Playing, Paused };
    GameState m_state = GameState::MainMenu;

    Graph m_uiGraph;
    std::unique_ptr<Executor> m_uiExecutor;
    std::unique_ptr<Context> m_uiContext;

    struct Platform {
        enum Type { Normal, OneTime, HighJump };
        float x, y, w, h;
        Type type = Normal;
    };
    struct Player { float x, y, vx, vy; float width, height; };
    Player m_player;
    std::vector<Platform> m_platforms;
    std::mt19937 m_rng;
    float m_cameraY = 0.0f;
    float m_prevPlayerY = 0.0f;
    int m_score = 0;
    bool m_gameOver = false;
    float m_maxHeight = 0.0f;

    bool m_leftPressed = false;
    bool m_rightPressed = false;

    std::shared_ptr<render::Texture> m_playerTex;
    std::shared_ptr<render::Texture> m_normalPlatformTex;
    std::shared_ptr<render::Texture> m_oneTimePlatformTex;
    std::shared_ptr<render::Texture> m_highJumpPlatformTex;
    std::shared_ptr<render::Mesh> m_quadMesh;
    std::shared_ptr<render::Material> m_gameMaterial;

    ma_engine m_audioEngine;
    ma_sound* m_currentSound = nullptr;
    ma_waveform* m_currentWaveform = nullptr;   // <-- добавлено

    void buildFullUIGraph();
    void resetGame();
    void generateInitialPlatforms();
    void updatePlatforms(float deltaTime);
    void checkCollisionsAndJump();
    void drawGameObjects();
    void stopCurrentSound();
};

} // namespace arxglue::game