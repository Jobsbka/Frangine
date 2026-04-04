#include "SceneGraph.hpp"
#include "core/utils/Logger.hpp"
#include <algorithm>

namespace frabgine::engine {

// GameObject implementation
GameObject::GameObject(const std::string& name) : name_(name) {}

GameObject::~GameObject() = default;

void GameObject::update(float deltaTime) {
    if (!active_) return;
    
    for (auto& component : components_) {
        if (component) {
            component->onUpdate(deltaTime);
        }
    }
}

void GameObject::render() {
    if (!active_) return;
    
    for (auto& component : components_) {
        if (component) {
            component->onRender();
        }
    }
}

// TransformComponent implementation
core::math::Matrix4x4 TransformComponent::getTransformMatrix() const {
    // Упрощенная реализация - в полной версии нужно использовать матрицы поворота и масштаба
    core::math::Matrix4x4 translation = core::math::Matrix4x4::translation(position_);
    core::math::Matrix4x4 rotation = core::math::Matrix4x4::rotation(rotation_);
    core::math::Matrix4x4 scale = core::math::Matrix4x4::scale(scale_);
    
    return translation * rotation * scale;
}

core::math::Matrix4x4 TransformComponent::getInverseTransformMatrix() const {
    return getTransformMatrix().inverse();
}

// CameraComponent implementation
CameraComponent::CameraComponent(float fov, float aspectRatio, float nearPlane, float farPlane)
    : fov_(fov), aspectRatio_(aspectRatio), nearPlane_(nearPlane), farPlane_(farPlane) {}

core::math::Matrix4x4 CameraComponent::getViewMatrix() const {
    if (!gameObject_) {
        return core::math::Matrix4x4::identity();
    }
    
    auto transform = gameObject_->getComponent<TransformComponent>();
    if (!transform) {
        return core::math::Matrix4x4::identity();
    }
    
    // Упрощенная реализация view матрицы
    const auto& pos = transform->getPosition();
    const auto& rot = transform->getRotation();
    
    return core::math::Matrix4x4::lookAt(
        pos,
        pos + core::math::Vector3f(0, 0, -1), // Смотрим вперед по Z
        core::math::Vector3f(0, 1, 0)          // Up по Y
    );
}

core::math::Matrix4x4 CameraComponent::getProjectionMatrix() const {
    return core::math::Matrix4x4::perspective(fov_, aspectRatio_, nearPlane_, farPlane_);
}

core::math::Matrix4x4 CameraComponent::getViewProjectionMatrix() const {
    return getProjectionMatrix() * getViewMatrix();
}

// Scene implementation
Scene::Scene(const std::string& name) : name_(name) {}

Scene::~Scene() = default;

GameObject* Scene::createGameObject(const std::string& name) {
    auto gameObject = std::make_unique<GameObject>(name);
    GameObject* ptr = gameObject.get();
    gameObjects_.push_back(std::move(gameObject));
    FRABGINE_LOG_DEBUG("Created game object: {}", name);
    return ptr;
}

void Scene::destroyGameObject(GameObject* gameObject) {
    auto it = std::find_if(gameObjects_.begin(), gameObjects_.end(),
        [gameObject](const GameObjectPtr& ptr) {
            return ptr.get() == gameObject;
        });
    
    if (it != gameObjects_.end()) {
        FRABGINE_LOG_DEBUG("Destroyed game object: {}", (*it)->getName());
        gameObjects_.erase(it);
    }
}

GameObject* Scene::findGameObjectByName(const std::string& name) {
    for (auto& gameObject : gameObjects_) {
        if (gameObject->getName() == name) {
            return gameObject.get();
        }
    }
    return nullptr;
}

std::vector<GameObject*> Scene::getAllGameObjects() {
    std::vector<GameObject*> result;
    result.reserve(gameObjects_.size());
    for (auto& gameObject : gameObjects_) {
        result.push_back(gameObject.get());
    }
    return result;
}

void Scene::update(float deltaTime) {
    if (!active_) return;
    
    for (auto& gameObject : gameObjects_) {
        if (gameObject && gameObject->isActive()) {
            gameObject->update(deltaTime);
        }
    }
}

void Scene::render() {
    if (!active_) return;
    
    for (auto& gameObject : gameObjects_) {
        if (gameObject && gameObject->isActive()) {
            gameObject->render();
        }
    }
}

// SceneManager implementation
SceneManager& SceneManager::getInstance() {
    static SceneManager instance;
    return instance;
}

Scene* SceneManager::createScene(const std::string& name) {
    auto scene = std::make_unique<Scene>(name);
    Scene* ptr = scene.get();
    scenes_.push_back(std::move(scene));
    
    if (!activeScene_) {
        activeScene_ = ptr;
    }
    
    FRABGINE_LOG_INFO("Created scene: {}", name);
    return ptr;
}

void SceneManager::destroyScene(Scene* scene) {
    auto it = std::find_if(scenes_.begin(), scenes_.end(),
        [scene](const std::unique_ptr<Scene>& ptr) {
            return ptr.get() == scene;
        });
    
    if (it != scenes_.end()) {
        if (activeScene_ == scene) {
            activeScene_ = nullptr;
        }
        
        FRABGINE_LOG_INFO("Destroyed scene: {}", scene->getName());
        scenes_.erase(it);
    }
}

void SceneManager::setActiveScene(Scene* scene) {
    activeScene_ = scene;
    FRABGINE_LOG_INFO("Set active scene: {}", scene ? scene->getName() : "none");
}

void SceneManager::update(float deltaTime) {
    if (activeScene_) {
        activeScene_->update(deltaTime);
    }
}

void SceneManager::render() {
    if (activeScene_) {
        activeScene_->render();
    }
}

} // namespace frabgine::engine
