#include "render_nodes.hpp"
#include "node_factory.hpp"
#include "../render/graphics_device.hpp"
#include <fstream>
#include <cstring>

namespace arxglue {

// Загрузка меша из файла (бинарный формат .3tm)
void LoadMeshNode::execute(Context& ctx) {
    auto& device = render::GraphicsDevice::instance();
    std::ifstream file(m_path, std::ios::binary);
    if (!file) {
        setOutputValue(ctx, 0, std::shared_ptr<render::Mesh>());
        return;
    }

    uint32_t vertexCount;
    file.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
    std::vector<float> vertices(vertexCount);
    file.read(reinterpret_cast<char*>(vertices.data()), vertexCount * sizeof(float));

    uint32_t indexCount;
    file.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
    std::vector<uint32_t> indices(indexCount);
    file.read(reinterpret_cast<char*>(indices.data()), indexCount * sizeof(uint32_t));

    uint32_t attribCount;
    file.read(reinterpret_cast<char*>(&attribCount), sizeof(attribCount));
    std::vector<int> attribSizes(attribCount);
    file.read(reinterpret_cast<char*>(attribSizes.data()), attribCount * sizeof(int));

    auto mesh = device.createMesh(vertices, indices, attribSizes);
    setOutputValue(ctx, 0, mesh);
}

// MaterialNode
void MaterialNode::execute(Context& ctx) {
    auto& device = render::GraphicsDevice::instance();

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

    auto shader = device.createShader(vertexSrc, fragmentSrc);
    auto material = device.createMaterial(shader);
    material->setParameter("uColor", m_color);
    setOutputValue(ctx, 0, material);
}

// SceneNode
void SceneNode::execute(Context& ctx) {
    auto scene = std::make_shared<render::Scene>();
    for (const auto& desc : m_renderables) {
        scene->addRenderable(desc.mesh, desc.material, desc.transform);
    }
    setOutputValue(ctx, 0, scene);
}

void SceneNode::setParameter(const std::string& name, const std::any& value) {
    (void)name;
    (void)value;
    // В будущем здесь будет реализация добавления объектов
}

std::any SceneNode::getParameter(const std::string& name) const {
    (void)name;
    return {};
}

void SceneNode::serialize(nlohmann::json& j) const {
    j["type"] = "Scene";
    nlohmann::json arr = nlohmann::json::array();
    // Сериализация renderables будет добавлена позже
    j["params"]["renderables"] = arr;
}

void SceneNode::deserialize(const nlohmann::json& j) {
    // Восстановление будет добавлено позже
    (void)j;
}

// CameraNode
void CameraNode::execute(Context& ctx) {
    auto camera = std::make_shared<render::Camera>();
    camera->lookAt(m_eye, m_center, m_up);
    auto* window = render::GraphicsDevice::instance().getWindow();
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float aspect = static_cast<float>(width) / height;
    const float pi = 3.1415926535f;
    camera->setPerspective(m_fov * pi / 180.0f, aspect, m_near, m_far);
    setOutputValue(ctx, 0, camera);
}

void CameraNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "eye") m_eye = std::any_cast<std::array<float,3>>(value);
    else if (name == "center") m_center = std::any_cast<std::array<float,3>>(value);
    else if (name == "up") m_up = std::any_cast<std::array<float,3>>(value);
    else if (name == "fov") m_fov = std::any_cast<float>(value);
    else if (name == "near") m_near = std::any_cast<float>(value);
    else if (name == "far") m_far = std::any_cast<float>(value);
    updateMatrix();
}

std::any CameraNode::getParameter(const std::string& name) const {
    if (name == "eye") return m_eye;
    if (name == "center") return m_center;
    if (name == "up") return m_up;
    if (name == "fov") return m_fov;
    if (name == "near") return m_near;
    if (name == "far") return m_far;
    return {};
}

void CameraNode::serialize(nlohmann::json& j) const {
    j["type"] = "Camera";
    j["params"]["eye"] = {m_eye[0], m_eye[1], m_eye[2]};
    j["params"]["center"] = {m_center[0], m_center[1], m_center[2]};
    j["params"]["up"] = {m_up[0], m_up[1], m_up[2]};
    j["params"]["fov"] = m_fov;
    j["params"]["near"] = m_near;
    j["params"]["far"] = m_far;
}

void CameraNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        auto& p = j["params"];
        if (p.contains("eye")) { auto arr = p["eye"]; m_eye = {arr[0], arr[1], arr[2]}; }
        if (p.contains("center")) { auto arr = p["center"]; m_center = {arr[0], arr[1], arr[2]}; }
        if (p.contains("up")) { auto arr = p["up"]; m_up = {arr[0], arr[1], arr[2]}; }
        if (p.contains("fov")) m_fov = p["fov"];
        if (p.contains("near")) m_near = p["near"];
        if (p.contains("far")) m_far = p["far"];
    }
    updateMatrix();
}

void CameraNode::updateMatrix() {
    // Матрицы будут вычислены в execute, так как нужен aspect
}

// RenderSceneNode
void RenderSceneNode::execute(Context& ctx) {
    auto scene = std::any_cast<std::shared_ptr<render::Scene>>(getInputValue(ctx, 0));
    auto camera = std::any_cast<std::shared_ptr<render::Camera>>(getInputValue(ctx, 1));
    if (!scene || !camera) return;

    render::CommandBuffer cmd;
    cmd.begin();
    cmd.setRenderTarget(nullptr);
    cmd.clear(0.2f, 0.3f, 0.3f, 1.0f);
    int width, height;
    auto* window = render::GraphicsDevice::instance().getWindow();
    glfwGetFramebufferSize(window, &width, &height);
    cmd.setViewport(0, 0, width, height);
    cmd.drawScene(*scene, *camera);
    cmd.end();
    cmd.execute();
}

// RenderToTextureNode
void RenderToTextureNode::execute(Context& ctx) {
    auto scene = std::any_cast<std::shared_ptr<render::Scene>>(getInputValue(ctx, 0));
    auto camera = std::any_cast<std::shared_ptr<render::Camera>>(getInputValue(ctx, 1));
    if (!scene || !camera) return;

    auto& device = render::GraphicsDevice::instance();
    auto target = device.createRenderTarget(m_width, m_height);

    render::CommandBuffer cmd;
    cmd.begin();
    cmd.setRenderTarget(target);
    cmd.clear(0.2f, 0.3f, 0.3f, 1.0f);
    cmd.setViewport(0, 0, m_width, m_height);
    cmd.drawScene(*scene, *camera);
    cmd.end();
    cmd.execute();

    setOutputValue(ctx, 0, target->getColorTexture());
}

void registerRenderNodes() {
    auto& factory = NodeFactory::instance();
    factory.registerNode("LoadMesh", []() { return std::make_unique<LoadMeshNode>(); });
    factory.registerNode("Material", []() { return std::make_unique<MaterialNode>(); });
    factory.registerNode("Scene", []() { return std::make_unique<SceneNode>(); });
    factory.registerNode("Camera", []() { return std::make_unique<CameraNode>(); });
    factory.registerNode("RenderScene", []() { return std::make_unique<RenderSceneNode>(); });
    factory.registerNode("RenderToTexture", []() { return std::make_unique<RenderToTextureNode>(); });
}

} // namespace arxglue