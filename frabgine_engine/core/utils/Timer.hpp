#ifndef FRABGINE_CORE_UTILS_TIMER_HPP
#define FRABGINE_CORE_UTILS_TIMER_HPP

#include <chrono>
#include <cstdint>

namespace frabgine {

class Timer {
private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    
    TimePoint startTime_;
    TimePoint lastTickTime_;
    bool running_ = false;
    
public:
    Timer() : startTime_(Clock::now()), lastTickTime_(startTime_) {}
    
    void start() {
        startTime_ = Clock::now();
        lastTickTime_ = startTime_;
        running_ = true;
    }
    
    void stop() {
        running_ = false;
    }
    
    void reset() {
        startTime_ = Clock::now();
        lastTickTime_ = startTime_;
    }
    
    // Возвращает время в секундах с момента запуска
    double elapsed() const {
        auto now = Clock::now();
        return std::chrono::duration<double>(now - startTime_).count();
    }
    
    // Возвращает время в миллисекундах с момента запуска
    uint64_t elapsedMs() const {
        auto now = Clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_).count();
    }
    
    // Возвращает время в микросекундах с момента запуска
    uint64_t elapsedUs() const {
        auto now = Clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now - startTime_).count();
    }
    
    // Возвращает дельту времени (время с последнего вызова tick)
    double tick() {
        auto now = Clock::now();
        double delta = std::chrono::duration<double>(now - lastTickTime_).count();
        lastTickTime_ = now;
        return delta;
    }
    
    bool isRunning() const { return running_; }
};

// Класс для измерения производительности (RAII)
class ScopedTimer {
private:
    const char* name_;
    Timer timer_;
    
public:
    explicit ScopedTimer(const char* name) : name_(name) {}
    
    ~ScopedTimer() {
        // Здесь можно добавить логирование времени выполнения
    }
    
    double elapsed() const { return timer_.elapsed(); }
};

} // namespace frabgine

#endif // FRABGINE_CORE_UTILS_TIMER_HPP
