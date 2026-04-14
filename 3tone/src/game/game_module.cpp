#include "game_module.hpp"
#include "Game.hpp"
#include "scene.hpp"
#include "game_object.hpp"
#include "../core/type_system.hpp"
#include "../nodes/node_factory.hpp"

namespace arxglue::game {

void initializeModule() {
    auto& ts = TypeSystem::instance();
    ts.registerType(TypeId::GameObject, typeid(std::shared_ptr<GameObject>));
    ts.registerType(TypeId::SceneGraph, typeid(std::shared_ptr<Scene>));
}

} // namespace arxglue::game