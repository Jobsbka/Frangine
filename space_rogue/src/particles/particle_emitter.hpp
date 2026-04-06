#pragma once
#include "../math/math.hpp"
#include <vector>

// Структура частицы
struct Particle {
    sVector30 position;
    sVector30 velocity;
    sVector30 color;
    float lifetime;      // оставшееся время жизни
    float maxLifetime;   // полное время жизни
    float size;
    bool active;

    Particle() : position(), velocity(), color(1,1,1), lifetime(0), maxLifetime(1), size(1.0f), active(false) {}
};

// Эмиттер частиц - генерирует и обновляет частицы
class ParticleEmitter {
public:
    ParticleEmitter(size_t maxParticles = 1024);
    
    // Обновить все активные частицы (dt - дельта времени в секундах)
    void update(float dt);
    
    // Испустить частицы из точки
    void emit(const sVector30& position, const sVector30& velocity, 
              const sVector30& color, float lifetime, float size, int count);
    
    // Испустить частицы с разбросом скоростей (для взрывов)
    void emitExplosion(const sVector30& position, const sVector30& color, 
                       float lifetime, float size, int count, float speed);
    
    // Получить массив частиц для рендеринга
    const std::vector<Particle>& getParticles() const { return particles; }
    
    // Количество активных частиц
    size_t getActiveCount() const;
    
private:
    std::vector<Particle> particles;
    size_t maxParticles;
    size_t firstFreeIndex;  // индекс для быстрого поиска свободного слота
};
