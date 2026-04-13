#include "graphics_device.hpp"
#include "../types/type_system.hpp"
#include "../nodes/node_factory.hpp"
#include "../nodes/render_nodes.hpp"

namespace arxglue::render {

void initializeModule() {
    // Регистрация рендер-типов
    auto& ts = TypeSystem::instance();
    ts.registerType(TypeId::Texture,   typeid(std::shared_ptr<render::Texture>));
    ts.registerType(TypeId::Mesh,      typeid(std::shared_ptr<render::Mesh>));
    ts.registerType(TypeId::Material,  typeid(std::shared_ptr<render::Material>));
    ts.registerType(TypeId::Scene,     typeid(std::shared_ptr<render::Scene>));
    ts.registerType(TypeId::Camera,    typeid(std::shared_ptr<render::Camera>));

    // Регистрация узлов рендеринга
    registerRenderNodes();
}

} // namespace arxglue::render