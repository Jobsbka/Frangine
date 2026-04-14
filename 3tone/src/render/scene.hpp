#pragma once
#include <vector>
#include <memory>
#include <array>
#include "mesh.hpp"
#include "material.hpp"

namespace arxglue::render {

struct Renderable {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    std::array<float, 16> transform; // model matrix
};

class Scene {
public:
    void addRenderable(std::shared_ptr<Mesh> mesh,
                       std::shared_ptr<Material> material,
                       const std::array<float, 16>& transform);
    void addRenderable(Renderable&& renderable);

    const std::vector<Renderable>& getRenderables() const { return m_renderables; }

private:
    std::vector<Renderable> m_renderables;
};

} // namespace arxglue::render