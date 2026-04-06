#include "particle_emitter.hpp"
#include <cstdlib>
#include <cmath>

ParticleEmitter::ParticleEmitter(size_t maxParticles) 
    : maxParticles(maxParticles), firstFreeIndex(0) {
    particles.resize(maxParticles);
}

void ParticleEmitter::update(float dt) {
    for (size_t i = 0; i < maxParticles; ++i) {
        if (particles[i].active) {
            // Обновляем позицию
            particles[i].position = particles[i].position + particles[i].velocity * dt;
            
            // Уменьшаем время жизни
            particles[i].lifetime -= dt;
            
            // Деактивируем, если время жизни истекло
            if (particles[i].lifetime <= 0.0f) {
                particles[i].active = false;
                if (i < firstFreeIndex) {
                    firstFreeIndex = i;
                }
            }
        }
    }
}

void ParticleEmitter::emit(const sVector30& position, const sVector30& velocity,
                           const sVector30& color, float lifetime, float size, int count) {
    for (int i = 0; i < count; ++i) {
        // Ищем свободный слот
        while (firstFreeIndex < maxParticles && particles[firstFreeIndex].active) {
            ++firstFreeIndex;
        }
        
        if (firstFreeIndex >= maxParticles) {
            // Нет свободных частиц, начинаем с начала (перезаписываем старые)
            firstFreeIndex = 0;
            while (firstFreeIndex < maxParticles && particles[firstFreeIndex].active) {
                ++firstFreeIndex;
            }
            if (firstFreeIndex >= maxParticles) {
                break; // Все частицы активны, не можем создать больше
            }
        }
        
        // Создаём частицу
        Particle& p = particles[firstFreeIndex];
        p.position = position;
        p.velocity = velocity;
        p.color = color;
        p.lifetime = lifetime;
        p.maxLifetime = lifetime;
        p.size = size;
        p.active = true;
        
        ++firstFreeIndex;
    }
}

void ParticleEmitter::emitExplosion(const sVector30& position, const sVector30& color,
                                    float lifetime, float size, int count, float speed) {
    for (int i = 0; i < count; ++i) {
        // Ищем свободный слот
        while (firstFreeIndex < maxParticles && particles[firstFreeIndex].active) {
            ++firstFreeIndex;
        }
        
        if (firstFreeIndex >= maxParticles) {
            firstFreeIndex = 0;
            while (firstFreeIndex < maxParticles && particles[firstFreeIndex].active) {
                ++firstFreeIndex;
            }
            if (firstFreeIndex >= maxParticles) {
                break;
            }
        }
        
        // Случайное направление для взрыва
        float theta = static_cast<float>(std::rand()) / RAND_MAX * 6.283185f; // 0..2*pi
        float phi = std::acos(2.0f * static_cast<float>(std::rand()) / RAND_MAX - 1.0f);
        
        sVector30 dir(
            std::sin(phi) * std::cos(theta),
            std::sin(phi) * std::sin(theta),
            std::cos(phi)
        );
        
        // Небольшой разброс скоростей
        float speedVar = speed * (0.5f + static_cast<float>(std::rand()) / RAND_MAX);
        
        Particle& p = particles[firstFreeIndex];
        p.position = position;
        p.velocity = dir * speedVar;
        p.color = color;
        p.lifetime = lifetime;
        p.maxLifetime = lifetime;
        p.size = size;
        p.active = true;
        
        ++firstFreeIndex;
    }
}

size_t ParticleEmitter::getActiveCount() const {
    size_t count = 0;
    for (size_t i = 0; i < maxParticles; ++i) {
        if (particles[i].active) {
            ++count;
        }
    }
    return count;
}
