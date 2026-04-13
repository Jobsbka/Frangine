#include "Game.hpp"
#include "scene.hpp"
#include "game_object.hpp"
#include "../types/type_system.hpp"
#include "../nodes/node_factory.hpp"

namespace arxglue::game {

void initializeModule() {
    auto& ts = TypeSystem::instance();
    ts.registerType(TypeId::GameObject, typeid(std::shared_ptr<GameObject>));
    ts.registerType(TypeId::Scene,      typeid(std::shared_ptr<Scene>));

    // Здесь можно зарегистрировать игровые узлы, если они появятся
    // auto& factory = NodeFactory::instance();
    // factory.registerNode("...", ...);
}

} // namespace arxglue::game