#include "scene.hpp"
#include "game_object.hpp"

namespace arxglue::game {

void Scene::addObject(std::shared_ptr<GameObject> obj) {
    m_objects.push_back(std::move(obj));
}

} // namespace arxglue::game