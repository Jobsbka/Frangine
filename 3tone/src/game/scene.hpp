#pragma once
#include <vector>
#include <memory>
#include <string>
#include "../core/arxglue.hpp"

namespace arxglue::game {

class GameObject;

class Scene {
public:
    void addObject(std::shared_ptr<GameObject> obj);
    const std::vector<std::shared_ptr<GameObject>>& getObjects() const { return m_objects; }
private:
    std::vector<std::shared_ptr<GameObject>> m_objects;
};

} // namespace arxglue::game