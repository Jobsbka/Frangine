#pragma once
#include "Game.hpp"
#include "../render/mesh.hpp"
#include "../render/material.hpp"
#include "../render/camera.hpp"
#include <vector>
#include <array>
#include <random>
#include "miniaudio.h"

namespace arxglue::game {

struct Platform {
    std::array<float, 3> position;
    std::array<float, 3> scale;
    std::array<float, 3> color;
    bool isBouncy;
    bool isBreakable;
};

class DoodleJumpGame : public Game {
public:
    DoodleJumpGame();
    ~DoodleJumpGame() override;

protected:
    void onInit() override;
    void onUpdate(float deltaTime) override;
    void onRender() override;
    void onKey(int key, int scancode, int action, int mods) override;

private:
    // Графика
    std::shared_ptr<render::Mesh> m_playerMesh;
    std::shared_ptr<render::Mesh> m_platformMesh;
    std::shared_ptr<render::Material> m_playerMaterial;
    std::shared_ptr<render::Material> m_platformMaterial;
    std::shared_ptr<render::Camera> m_camera;

    // Игровое состояние
    std::array<float, 3> m_playerPos = {0,0,0};
    std::array<float, 3> m_playerVel = {0,0,0};
    std::vector<Platform> m_platforms;
    float m_score = 0.0f;
    bool m_moveLeft = false, m_moveRight = false;
    std::mt19937 m_rng;

    // Аудио (miniaudio high-level)
    ma_engine m_engine;
    ma_waveform m_jumpWaveform;
    ma_sound m_jumpSound;
    float m_jumpSoundTimer = 0.0f;   // <-- ДОБАВЛЕНО

    void updatePhysics(float dt);
    void spawnNewPlatforms();
};

} // namespace arxglue::game