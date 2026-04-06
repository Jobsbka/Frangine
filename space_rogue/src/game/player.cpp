#include "player.hpp"

Player::Player() 
    : position(0, 0, 0), 
      velocity(), 
      fireCooldown(0) {
    projectiles.reserve(64);
}

void Player::update(float dt) {
    // Обновляем позицию
    position = position + velocity * dt;
    
    // Обновляем снаряды
    for (auto& proj : projectiles) {
        if (proj.active) {
            proj.position = proj.position + proj.velocity * dt;
            proj.lifetime -= dt;
            if (proj.lifetime <= 0) {
                proj.active = false;
            }
        }
    }
    
    // Кулдаун стрельбы
    if (fireCooldown > 0) {
        fireCooldown -= dt;
    }
}

void Player::fire() {
    if (fireCooldown > 0) return;
    
    // Ищем свободный снаряд или создаём новый
    Projectile* freeProj = nullptr;
    for (auto& proj : projectiles) {
        if (!proj.active) {
            freeProj = &proj;
            break;
        }
    }
    
    if (!freeProj) {
        projectiles.emplace_back();
        freeProj = &projectiles.back();
    }
    
    // Настраиваем снаряд - стреляем вперёд по Z
    freeProj->position = position;
    freeProj->velocity = sVector30(0, 0, PROJECTILE_SPEED);
    freeProj->lifetime = 2.0f;  // 2 секунды жизни
    freeProj->active = true;
    
    fireCooldown = FIRE_COOLDOWN_TIME;
}

void Player::move(const sVector30& direction, float dt) {
    velocity = direction * MOVE_SPEED;
}
