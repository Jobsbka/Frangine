// src/game/Game.cpp
#include "Game.hpp"
#include "../render/command_buffer.hpp"
#include "../render/texture.hpp"
#include "../nodes/basic_nodes.hpp"
#include "../nodes/checker_texture.hpp"
#include "../nodes/asset_node.hpp"
#include "../nodes/convert_node.hpp"
#include "../nodes/text_texture_node.hpp"
#include "../nodes/node_factory.hpp"
#include "../types/type_system.hpp"
#include <GLFW/glfw3.h>
#include <cmath>
#include <chrono>
#include <random>
#include <algorithm>
#include <sstream>

namespace arxglue::game {

// ========== Узел, читающий float из state ==========
class GetStateFloatNode : public INode {
public:
    GetStateFloatNode(const std::string& key) : m_key(key) {}
    void execute(Context& ctx) override {
        float val = ctx.getState<float>(m_key);
        setOutputValue(ctx, 0, val);
    }
    ComponentMetadata getMetadata() const override {
        return {"GetStateFloat", {}, {{"value", typeid(float)}}, true, false};
    }
    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "key") m_key = std::any_cast<std::string>(value);
    }
    std::any getParameter(const std::string& name) const override {
        if (name == "key") return m_key;
        return {};
    }
    void serialize(nlohmann::json& j) const override {
        j["type"] = "GetStateFloat";
        j["params"]["key"] = m_key;
    }
    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params") && j["params"].contains("key"))
            m_key = j["params"]["key"].get<std::string>();
    }
private:
    std::string m_key;
};
static bool registerGetStateFloatNode() {
    NodeFactory::instance().registerNode("GetStateFloat", []() { return std::make_unique<GetStateFloatNode>(""); });
    return true;
}
static bool dummyGetStateFloat = registerGetStateFloatNode();

// ========== Узел Y в цвет ==========
class YToColorNode : public INode {
public:
    void execute(Context& ctx) override {
        float y = std::any_cast<float>(getInputValue(ctx, 0));
        float r = (std::sin(y) + 1.0f) * 0.5f;
        float g = (std::cos(y * 1.3f) + 1.0f) * 0.5f;
        float b = (std::sin(y * 2.1f) + 1.0f) * 0.5f;
        setOutputValue(ctx, 0, std::array<float,3>{r, g, b});
    }
    ComponentMetadata getMetadata() const override {
        return {"YToColor", {{"y", typeid(float)}}, {{"color", typeid(std::array<float,3>)}}, true, false};
    }
    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "YToColor"; }
    void deserialize(const nlohmann::json&) override {}
};
static bool registerYToColorNode() {
    NodeFactory::instance().registerNode("YToColor", [](){ return std::make_unique<YToColorNode>(); });
    return true;
}
static bool dummyYToColor = registerYToColorNode();

// ========== Создание мешей ==========
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
    auto& device = render::GraphicsDevice::instance();
    return device.createMesh(vertices, indices, attribSizes);
}

static std::shared_ptr<render::Mesh> createPlatformMesh(float width, float height, float depth) {
    float w2 = width * 0.5f;
    float h2 = height * 0.5f;
    float d2 = depth * 0.5f;
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
    auto& device = render::GraphicsDevice::instance();
    return device.createMesh(vertices, indices, attribSizes);
}

static std::shared_ptr<render::Mesh> createQuadMesh(float width, float height) {
    float w = width * 0.5f;
    float h = height * 0.5f;
    std::vector<float> vertices = {
        -w, -h, 0.0f, 0,0,1, 0,0,   w, -h, 0.0f, 0,0,1, 1,0,
         w,  h, 0.0f, 0,0,1, 1,1,  -w,  h, 0.0f, 0,0,1, 0,1
    };
    std::vector<uint32_t> indices = {0,1,2, 2,3,0};
    std::vector<int> attribSizes = {3, 3, 2};
    auto& device = render::GraphicsDevice::instance();
    return device.createMesh(vertices, indices, attribSizes);
}

// ========== Конструктор ==========
Game::Game(GLFWwindow* window) : m_window(window), m_rng(std::random_device{}()) {
    auto& device = render::GraphicsDevice::instance();
    device.initialize(window);

    m_playerMesh = createCubeMesh(0.4f);
    m_platformMesh = createPlatformMesh(1.2f, 0.2f, 0.8f);
    m_quadMesh = createQuadMesh(2.5f, 0.8f);  // увеличен под шрифт

    // Основной шейдер для 3D объектов
    auto shader = device.createShader(
        R"(
            #version 450 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec3 aNormal;
            layout(location = 2) in vec2 aUV;
            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProjection;
            out vec3 vNormal;
            out vec3 vFragPos;
            out vec2 vUV;
            void main() {
                vFragPos = vec3(uModel * vec4(aPos, 1.0));
                vNormal = mat3(transpose(inverse(uModel))) * aNormal;
                vUV = aUV;
                gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
            }
        )",
        R"(
            #version 450 core
            in vec3 vNormal;
            in vec3 vFragPos;
            in vec2 vUV;
            uniform vec3 uColor;
            uniform vec3 uLightDir;
            uniform sampler2D uTexture;
            uniform bool uUseTexture;
            out vec4 FragColor;
            void main() {
                vec3 normal = normalize(vNormal);
                float diff = max(dot(normal, normalize(uLightDir)), 0.0);
                vec3 ambient = vec3(0.2);
                vec3 texColor = uUseTexture ? texture(uTexture, vUV).rgb : vec3(1.0);
                vec3 result = (ambient + diff) * uColor * texColor;
                FragColor = vec4(result, 1.0);
            }
        )"
    );

    m_playerMaterial = device.createMaterial(shader);
    m_playerMaterial->setParameter("uColor", std::array<float,3>{0.2f, 0.6f, 1.0f});
    m_playerMaterial->setParameter("uLightDir", std::array<float,3>{1.0f, 2.0f, 1.0f});
    m_playerMaterial->setParameter("uUseTexture", false);

    m_platformMaterialBase = device.createMaterial(shader);
    m_platformMaterialBase->setParameter("uLightDir", std::array<float,3>{1.0f, 2.0f, 1.0f});
    m_platformMaterialBase->setParameter("uUseTexture", true);

    // Шейдер для UI (просто выводит текстуру без изменений)
    auto uiShader = device.createShader(
        R"(
            #version 450 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec3 aNormal;
            layout(location = 2) in vec2 aUV;
            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProjection;
            out vec2 vUV;
            void main() {
                vUV = aUV;
                gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
            }
        )",
        R"(
            #version 450 core
            in vec2 vUV;
            uniform sampler2D uTexture;
            uniform vec3 uColor;
            out vec4 FragColor;
            void main() {
                vec4 texColor = texture(uTexture, vUV);
                FragColor = texColor;  // белые цифры на чёрном фоне
            }
        )"
    );
    m_scoreMaterial = device.createMaterial(uiShader);
    m_scoreMaterial->setParameter("uColor", std::array<float,3>{1.0f, 1.0f, 1.0f});

    // Запекаем шахматную текстуру
    {
        Graph texGraph;
        auto checker = std::make_unique<CheckerTextureNode>(512, 512, 8, 8);
        NodeId checkerId = texGraph.addNode(std::move(checker));
        Executor exec(1);
        Context ctx;
        exec.execute(texGraph, ctx, {checkerId});
        auto texAsset = std::any_cast<std::shared_ptr<TextureAsset>>(ctx.output);
        if (texAsset) {
            m_bakedCheckerTexture = device.createTexture(512, 512, 4, texAsset->pixels.data());
            m_platformMaterialBase->setTexture("uTexture", m_bakedCheckerTexture);
        }
    }

    // Граф для цвета платформ
    initProceduralColorGraph();

    // Граф для текста счёта
    m_textGraph = std::make_unique<Graph>();
    m_textExecutor = std::make_unique<Executor>(1);
    auto textNode = std::make_unique<TextTextureNode>(512, 128, "0", 255, 255, 255);
    m_textGraphOutputId = m_textGraph->addNode(std::move(textNode));

    // Начальные платформы
    for (int i = 0; i < 8; ++i) {
        float y = -2.0f + i * 1.2f;
        float x = (m_rng() % 100) / 100.0f * 4.0f - 2.0f;
        Platform plat;
        plat.position = {x, y, 0.0f};
        plat.scale = {1.2f, 0.2f, 0.8f};
        plat.isBouncy = (std::abs(y) < 0.1f);
        plat.isBreakable = (y > 3.0f);
        plat.color = computePlatformColor(y, plat.isBouncy, plat.isBreakable);
        m_platforms.push_back(plat);
    }

    m_scoreTexture = nullptr;
    m_lastScoreRendered = -1.0f;
}

Game::~Game() = default;

void Game::initProceduralColorGraph() {
    m_colorGraph = std::make_unique<Graph>();
    m_colorExecutor = std::make_unique<Executor>(1);
    auto getY = std::make_unique<GetStateFloatNode>("platformY");
    m_colorGraphYInputId = m_colorGraph->addNode(std::move(getY));
    auto yToColor = std::make_unique<YToColorNode>();
    m_colorGraphOutputId = m_colorGraph->addNode(std::move(yToColor));
    m_colorGraph->addConnection({m_colorGraphYInputId, 0, m_colorGraphOutputId, 0});
}

std::array<float,3> Game::computePlatformColor(float y, bool isBouncy, bool isBreakable) {
    if (isBouncy) return {0.2f, 0.9f, 0.3f};
    if (isBreakable) return {0.9f, 0.2f, 0.2f};
    m_colorCtx.setState("platformY", y);
    m_colorExecutor->execute(*m_colorGraph, m_colorCtx, {m_colorGraphOutputId});
    if (m_colorCtx.output.has_value() && m_colorCtx.output.type() == typeid(std::array<float,3>))
        return std::any_cast<std::array<float,3>>(m_colorCtx.output);
    return {0.7f, 0.5f, 0.3f};
}

void Game::updateScoreTexture() {
    int scoreInt = (int)m_score;
    if (scoreInt == (int)m_lastScoreRendered) return;
    m_lastScoreRendered = (float)scoreInt;
    std::string text = std::to_string(scoreInt);
    INode* textNode = m_textGraph->getNode(m_textGraphOutputId);
    if (textNode) {
        textNode->setParameter("text", text);
        textNode->setDirty(true);
    }
    m_textExecutor->execute(*m_textGraph, m_textCtx, {m_textGraphOutputId});
    auto texAsset = std::any_cast<std::shared_ptr<TextureAsset>>(m_textCtx.output);
    if (texAsset) {
        auto& device = render::GraphicsDevice::instance();
        if (m_scoreTexture)
            m_scoreTexture->updateData(texAsset->pixels.data());
        else {
            m_scoreTexture = device.createTexture(texAsset->width, texAsset->height, 4, texAsset->pixels.data());
            m_scoreMaterial->setTexture("uTexture", m_scoreTexture);
        }
    }
}

void Game::processInput(float dt) {
    (void)dt;
    if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_LEFT) == GLFW_PRESS)
        m_moveLeft = true;
    else if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        m_moveRight = true;
    else {
        m_moveLeft = false;
        m_moveRight = false;
    }
    float speed = 5.0f;
    if (m_moveLeft) m_playerVel[0] = -speed;
    else if (m_moveRight) m_playerVel[0] = speed;
    else m_playerVel[0] *= 0.95f;
}

void Game::updatePhysics(float dt) {
    const float gravity = -12.0f;
    m_playerVel[1] += gravity * dt;
    float newY = m_playerPos[1] + m_playerVel[1] * dt;

    float playerHalfSize = 0.2f;
    float playerBottomOld = m_playerPos[1] - playerHalfSize;
    float playerBottomNew = newY - playerHalfSize;

    bool collided = false;
    float collisionY = 0.0f;
    Platform* collidedPlat = nullptr;

    std::lock_guard<std::mutex> lock(m_platformsMutex);
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
    } else {
        m_playerPos[1] = newY;
    }

    m_playerPos[0] += m_playerVel[0] * dt;
    float boundX = 3.5f;
    if (m_playerPos[0] < -boundX) m_playerPos[0] = -boundX;
    if (m_playerPos[0] > boundX) m_playerPos[0] = boundX;

    m_score = m_playerPos[1] + 1.0f;
    if (m_score > m_maxScore) m_maxScore = m_score;

    std::stringstream ss;
    ss << "3Tone Doodle Jump | Height: " << (int)m_score << " | Best: " << (int)m_maxScore;
    glfwSetWindowTitle(m_window, ss.str().c_str());

    if (m_playerPos[1] < -5.0f) {
        m_playerPos = {0.0f, 0.0f, 0.0f};
        m_playerVel = {0.0f, 0.0f, 0.0f};
        m_score = 0.0f;
        m_platforms.clear();
        for (int i = 0; i < 8; ++i) {
            float y = -2.0f + i * 1.2f;
            float x = (m_rng() % 100) / 100.0f * 4.0f - 2.0f;
            Platform plat;
            plat.position = {x, y, 0.0f};
            plat.scale = {1.2f, 0.2f, 0.8f};
            plat.isBouncy = (std::abs(y) < 0.1f);
            plat.isBreakable = (y > 3.0f);
            plat.color = computePlatformColor(y, plat.isBouncy, plat.isBreakable);
            m_platforms.push_back(plat);
        }
    }
}

void Game::generatePlatformsProcedurally(float playerY) {
    if (playerY > 2.0f && !m_genRequested) {
        m_genRequested = true;
        float topY = -1000.0f;
        {
            std::lock_guard<std::mutex> lock(m_platformsMutex);
            for (auto& p : m_platforms)
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
            newPlat.color = computePlatformColor(newY, newPlat.isBouncy, newPlat.isBreakable);
            std::lock_guard<std::mutex> lock(m_platformsMutex);
            m_platforms.push_back(newPlat);
        }
        {
            std::lock_guard<std::mutex> lock(m_platformsMutex);
            m_platforms.erase(std::remove_if(m_platforms.begin(), m_platforms.end(),
                [this](const Platform& p) { return p.position[1] < m_playerPos[1] - 4.0f; }),
                m_platforms.end());
        }
        m_genRequested = false;
    }
}

void Game::updateScene() {
    render::Scene scene;

    // Игрок
    std::array<float,16> playerMat = {
        1,0,0,0, 0,1,0,0, 0,0,1,0,
        m_playerPos[0], m_playerPos[1], m_playerPos[2], 1
    };
    scene.addRenderable(m_playerMesh, m_playerMaterial, playerMat);

    // Платформы
    {
        std::lock_guard<std::mutex> lock(m_platformsMutex);
        for (const auto& plat : m_platforms) {
            if (plat.scale[0] == 0.0f) continue;
            std::array<float,16> mat = {
                plat.scale[0],0,0,0,
                0,plat.scale[1],0,0,
                0,0,plat.scale[2],0,
                plat.position[0], plat.position[1], plat.position[2], 1
            };
            auto matInstance = render::GraphicsDevice::instance().createMaterial(m_platformMaterialBase->getShader());
            matInstance->setTexture("uTexture", m_bakedCheckerTexture);
            matInstance->setParameter("uColor", plat.color);
            matInstance->setParameter("uLightDir", std::array<float,3>{1.0f, 2.0f, 1.0f});
            matInstance->setParameter("uUseTexture", true);
            scene.addRenderable(m_platformMesh, matInstance, mat);
        }
    }

    // Счётчик над игроком
    updateScoreTexture();
    if (m_scoreTexture) {
        float quadW = 2.5f;
        float quadH = 0.8f;
        std::array<float,3> pos = {m_playerPos[0], m_playerPos[1] + 1.8f, -0.1f};  // Z = -0.1, чтобы быть перед платформами
        std::array<float,16> scoreMat = {
            quadW,0,0,0,
            0,quadH,0,0,
            0,0,1,0,
            pos[0], pos[1], pos[2], 1
        };
        m_scoreMaterial->setParameter("uColor", std::array<float,3>{1.0f, 1.0f, 1.0f});
        scene.addRenderable(m_quadMesh, m_scoreMaterial, scoreMat);
    }

    // Камера
    int w, h;
    glfwGetFramebufferSize(m_window, &w, &h);
    float aspect = (float)w / h;
    m_camera.setPerspective(45.0f * 3.14159f/180.0f, aspect, 0.1f, 100.0f);
    m_camera.lookAt({0.0f, m_playerPos[1] + 2.0f, 8.0f}, {0.0f, m_playerPos[1], 0.0f}, {0.0f, 1.0f, 0.0f});

    // Градиент фона каждые 100 единиц
    float height = m_playerPos[1] + 1.0f;
    int segment = (int)(height / 100.0f);
    float t = fmod(height, 100.0f) / 100.0f;
    float r_base = 0.1f + (segment % 3) * 0.3f;
    float g_base = 0.2f + ((segment+1) % 3) * 0.3f;
    float b_base = 0.3f + ((segment+2) % 3) * 0.3f;
    float r_next = 0.1f + ((segment+1) % 3) * 0.3f;
    float g_next = 0.2f + ((segment+2) % 3) * 0.3f;
    float b_next = 0.3f + ((segment+3) % 3) * 0.3f;
    float r = r_base * (1.0f - t) + r_next * t;
    float g = g_base * (1.0f - t) + g_next * t;
    float b = b_base * (1.0f - t) + b_next * t;
    if (r > 1.0f) r = 1.0f;
    if (g > 1.0f) g = 1.0f;
    if (b > 1.0f) b = 1.0f;

    render::CommandBuffer cmd;
    cmd.begin();
    cmd.setRenderTarget(nullptr);
    cmd.clear(r, g, b, 1.0f);
    cmd.setViewport(0, 0, w, h);
    cmd.drawScene(scene, m_camera);
    cmd.end();
    cmd.execute();
}

void Game::run() {
    auto lastTime = std::chrono::high_resolution_clock::now();
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        if (dt > 0.033f) dt = 0.033f;
        lastTime = now;
        processInput(dt);
        updatePhysics(dt);
        generatePlatformsProcedurally(m_playerPos[1]);
        updateScene();
        render::GraphicsDevice::instance().swapBuffers();
    }
}

} // namespace arxglue::game