#pragma once
#include "../math/math.hpp"

// Снаряд (луч/пуля)
struct Projectile {
    sVector30 position;
    sVector30 velocity;
    float lifetime;
    bool active;
    
    Projectile() : position(), velocity(), lifetime(0), active(false) {}
};

// Класс игрока
class Player {
public:
    Player();
    
    // Обновить состояние игрока
    void update(float dt);
    
    // Выстрелить
    void fire();
    
    // Получить позицию игрока
    const sVector31& getPosition() const { return position; }
    
    // Получить снаряды
    std::vector<Projectile>& getProjectiles() { return projectiles; }
    const std::vector<Projectile>& getProjectiles() const { return projectiles; }
    
    // Движение
    void move(const sVector30& direction, float dt);
    
private:
    sVector31 position;
    sVector30 velocity;
    std::vector<Projectile> projectiles;
    
    float fireCooldown;
    static constexpr float FIRE_COOLDOWN_TIME = 0.2f;
    static constexpr float PROJECTILE_SPEED = 50.0f;
    static constexpr float MOVE_SPEED = 15.0f;
};
