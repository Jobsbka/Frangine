#pragma once

#include "core/memory/SmartPointers.hpp"
#include <string>
#include <vector>
#include <functional>

namespace frabgine::engine {

// Базовый класс компонента
class Component {
public:
    virtual ~Component() = default;
    virtual void onUpdate(float deltaTime) {}
    virtual void onRender() {}
    
    class GameObject* getGameObject() const { return gameObject_; }
    void setGameObject(GameObject* gameObject) { gameObject_ = gameObject; }
    
protected:
    GameObject* gameObject_ = nullptr;
};

// Игровой объект в сцене
class GameObject {
public:
    using ComponentPtr = std::unique_ptr<Component>;
    
    GameObject(const std::string& name = "GameObject");
    ~GameObject();
    
    // Управление компонентами
    template<typename T, typename... Args>
    T* addComponent(Args&&... args) {
        static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
        
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        component->setGameObject(this);
        T* ptr = component.get();
        components_.push_back(std::move(component));
        return ptr;
    }
    
    template<typename T>
    T* getComponent() {
        for (auto& comp : components_) {
            if (auto typedComp = dynamic_cast<T*>(comp.get())) {
                return typedComp;
            }
        }
        return nullptr;
    }
    
    template<typename T>
    bool hasComponent() const {
        for (const auto& comp : components_) {
            if (dynamic_cast<const T*>(comp.get())) {
                return true;
            }
        }
        return false;
    }
    
    // Обновление и рендеринг
    void update(float deltaTime);
    void render();
    
    // Свойства
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    
    bool isActive() const { return active_; }
    void setActive(bool active) { active_ = active; }
    
    // Трансформация (будет реализована в TransformComponent)
    // Для MVP используем упрощенный подход
    
private:
    std::string name_;
    std::vector<ComponentPtr> components_;
    bool active_ = true;
};

// Transform компонент
class TransformComponent : public Component {
public:
    TransformComponent() = default;
    
    void setPosition(const core::math::Vector3f& pos) { position_ = pos; }
    void setRotation(const core::math::Vector3f& rot) { rotation_ = rot; }
    void setScale(const core::math::Vector3f& scale) { scale_ = scale; }
    
    const core::math::Vector3f& getPosition() const { return position_; }
    const core::math::Vector3f& getRotation() const { return rotation_; }
    const core::math::Vector3f& getScale() const { return scale_; }
    
    core::math::Matrix4x4 getTransformMatrix() const;
    core::math::Matrix4x4 getInverseTransformMatrix() const;
    
private:
    core::math::Vector3f position_{0.0f, 0.0f, 0.0f};
    core::math::Vector3f rotation_{0.0f, 0.0f, 0.0f};
    core::math::Vector3f scale_{1.0f, 1.0f, 1.0f};
};

// Camera компонент
class CameraComponent : public Component {
public:
    CameraComponent(float fov = 60.0f, float aspectRatio = 16.0f/9.0f, 
                    float nearPlane = 0.1f, float farPlane = 1000.0f);
    
    void setFOV(float fov) { fov_ = fov; }
    void setAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }
    void setNearPlane(float nearPlane) { nearPlane_ = nearPlane; }
    void setFarPlane(float farPlane) { farPlane_ = farPlane; }
    
    float getFOV() const { return fov_; }
    float getAspectRatio() const { return aspectRatio_; }
    float getNearPlane() const { return nearPlane_; }
    float getFarPlane() const { return farPlane_; }
    
    core::math::Matrix4x4 getViewMatrix() const;
    core::math::Matrix4x4 getProjectionMatrix() const;
    core::math::Matrix4x4 getViewProjectionMatrix() const;
    
private:
    float fov_;
    float aspectRatio_;
    float nearPlane_;
    float farPlane_;
};

// Scene - контейнер для игровых объектов
class Scene {
public:
    using GameObjectPtr = std::unique_ptr<GameObject>;
    
    Scene(const std::string& name = "Scene");
    ~Scene();
    
    // Создание и удаление объектов
    GameObject* createGameObject(const std::string& name = "GameObject");
    void destroyGameObject(GameObject* gameObject);
    
    // Поиск объектов
    GameObject* findGameObjectByName(const std::string& name);
    std::vector<GameObject*> getAllGameObjects();
    
    // Обновление сцены
    void update(float deltaTime);
    void render();
    
    // Свойства
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    
    bool isActive() const { return active_; }
    void setActive(bool active) { active_ = active; }
    
private:
    std::string name_;
    std::vector<GameObjectPtr> gameObjects_;
    bool active_ = true;
};

// Менеджер сцен
class SceneManager {
public:
    static SceneManager& getInstance();
    
    Scene* createScene(const std::string& name = "Scene");
    void destroyScene(Scene* scene);
    
    Scene* getActiveScene() { return activeScene_; }
    void setActiveScene(Scene* scene);
    
    void update(float deltaTime);
    void render();
    
private:
    SceneManager() = default;
    ~SceneManager() = default;
    
    std::vector<std::unique_ptr<Scene>> scenes_;
    Scene* activeScene_ = nullptr;
};

} // namespace frabgine::engine
