#include "command_buffer.hpp"
#include "graphics_device.hpp"
#include <glad/glad.h>

namespace arxglue::render {

struct DrawData {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    std::array<float, 16> transform;
};

struct SceneData {
    const Scene* scene;
    const Camera* camera;
};

void CommandBuffer::begin() {
    m_commands.push_back({Command::Begin, {}});
}

void CommandBuffer::end() {
    m_commands.push_back({Command::End, {}});
}

void CommandBuffer::setRenderTarget(std::shared_ptr<RenderTarget> target) {
    m_commands.push_back({Command::SetRenderTarget, target});
    m_currentTarget = target;
}

void CommandBuffer::clear(float r, float g, float b, float a) {
    std::array<float, 4> color = {r, g, b, a};
    m_commands.push_back({Command::Clear, color});
}

void CommandBuffer::setViewport(int x, int y, int width, int height) {
    std::array<int, 4> vp = {x, y, width, height};
    m_commands.push_back({Command::SetViewport, vp});
}

void CommandBuffer::drawMesh(std::shared_ptr<Mesh> mesh,
                             std::shared_ptr<Material> material,
                             const std::array<float, 16>& transform) {
    DrawData data{mesh, material, transform};
    m_commands.push_back({Command::DrawMesh, data});
}

void CommandBuffer::drawScene(const Scene& scene, const Camera& camera) {
    SceneData data{&scene, &camera};
    m_commands.push_back({Command::DrawScene, data});
}

void CommandBuffer::execute() {
    auto& device = GraphicsDevice::instance();
    for (const auto& cmd : m_commands) {
        switch (cmd.type) {
        case Command::Begin:
            break;
        case Command::End:
            break;
        case Command::SetRenderTarget: {
            auto target = std::any_cast<std::shared_ptr<RenderTarget>>(cmd.data);
            if (target) {
                target->bind();
            } else {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
            break;
        }
        case Command::Clear: {
            auto color = std::any_cast<std::array<float, 4>>(cmd.data);
            device.clear(color[0], color[1], color[2], color[3]);
            break;
        }
        case Command::SetViewport: {
            auto vp = std::any_cast<std::array<int, 4>>(cmd.data);
            device.setViewport(vp[0], vp[1], vp[2], vp[3]);
            break;
        }
        case Command::DrawMesh: {
            auto data = std::any_cast<DrawData>(cmd.data);
            if (data.mesh && data.material) {
                data.material->apply();
                data.material->getShader()->setUniformMat4("uModel", data.transform.data());
                data.mesh->draw();
            }
            break;
        }
        case Command::DrawScene: {
            auto data = std::any_cast<SceneData>(cmd.data);
            if (data.scene && data.camera) {
                for (const auto& r : data.scene->getRenderables()) {
                    if (r.mesh && r.material) {
                        r.material->apply();
                        r.material->getShader()->setUniformMat4("uView", data.camera->getViewMatrix());
                        r.material->getShader()->setUniformMat4("uProjection", data.camera->getProjectionMatrix());
                        r.material->getShader()->setUniformMat4("uModel", r.transform.data());
                        r.mesh->draw();
                    }
                }
            }
            break;
        }
        }
    }
    m_commands.clear();
}

} // namespace arxglue::render