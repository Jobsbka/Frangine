#pragma once
#include <string>
#include <memory>
#include <vector>
#include <any>
#include "../core/arxglue.hpp"

namespace arxglue::game {

class GameObject {
public:
    explicit GameObject(const std::string& name = "GameObject");
    virtual ~GameObject() = default;

    virtual void update(float deltaTime) {}
    const std::string& getName() const { return m_name; }
private:
    std::string m_name;
};

} // namespace arxglue::game