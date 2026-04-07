#include "scene.hpp"

namespace arxglue::render {

void Scene::addRenderable(std::shared_ptr<Mesh> mesh,
                          std::shared_ptr<Material> material,
                          const std::array<float, 16>& transform) {
    m_renderables.push_back({mesh, material, transform});
}

void Scene::addRenderable(Renderable&& renderable) {
    m_renderables.push_back(std::move(renderable));
}

} // namespace arxglue::render