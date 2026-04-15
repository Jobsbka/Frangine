#pragma once
#include <functional>
#include <vector>
#include <memory>
#include "scene.hpp"
#include "camera.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "render_target.hpp"

namespace arxglue::render {
	
struct DrawData {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    std::array<float, 16> transform;
    std::shared_ptr<Texture> texture; // <-- добавлено
};

class CommandBuffer {
public:
    void begin();
    void end();

    void setRenderTarget(std::shared_ptr<RenderTarget> target);
    void clear(float r, float g, float b, float a);
    void setViewport(int x, int y, int width, int height);
    void drawMesh(std::shared_ptr<Mesh> mesh,
                  std::shared_ptr<Material> material,
                  const std::array<float, 16>& transform,
                  std::shared_ptr<Texture> texture = nullptr);
    void drawScene(const Scene& scene, const Camera& camera);

    // Выполнить все записанные команды
    void execute();

private:
    struct Command {
        enum Type { Begin, End, SetRenderTarget, Clear, SetViewport, DrawMesh, DrawScene };
        Type type;
        std::any data;
    };

    std::vector<Command> m_commands;
    std::shared_ptr<RenderTarget> m_currentTarget;
};

} // namespace arxglue::render