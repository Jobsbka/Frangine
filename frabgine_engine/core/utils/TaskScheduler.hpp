#ifndef FRABGINE_CORE_UTILS_TASKSCHEDULER_HPP
#define FRABGINE_CORE_UTILS_TASKSCHEDULER_HPP

#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <future>
#include <atomic>

namespace frabgine {

class TaskScheduler {
private:
    using Task = std::function<void()>;
    
    std::vector<std::thread> workers_;
    std::queue<Task> tasks_;
    
    std::mutex queueMutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
    
    void workerThread() {
        while (true) {
            Task task;
            
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                condition_.wait(lock, [this] { 
                    return stop_.load() || !tasks_.empty(); 
                });
                
                if (stop_.load() && tasks_.empty()) return;
                
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            
            task();
        }
    }
    
public:
    explicit TaskScheduler(size_t threadCount = std::thread::hardware_concurrency()) {
        for (size_t i = 0; i < threadCount; ++i) {
            workers_.emplace_back(&TaskScheduler::workerThread, this);
        }
    }
    
    ~TaskScheduler() {
        stop_.store(true);
        condition_.notify_all();
        
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    template<typename F>
    void enqueue(F&& f) {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            tasks_.emplace(std::forward<F>(f));
        }
        condition_.notify_one();
    }
    
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using ReturnType = decltype(f(args...));
        
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<ReturnType> result = task->get_future();
        
        enqueue([task]() { (*task)(); });
        
        return result;
    }
    
    size_t getWorkerCount() const { return workers_.size(); }
    size_t getPendingTasks() const { 
        std::lock_guard<std::mutex> lock(queueMutex_);
        return tasks_.size();
    }
};

// Глобальный экземпляр планировщика
inline TaskScheduler* gTaskScheduler = nullptr;

inline void initTaskScheduler(size_t threadCount = 0) {
    if (!gTaskScheduler) {
        gTaskScheduler = new TaskScheduler(threadCount);
    }
}

inline void shutdownTaskScheduler() {
    delete gTaskScheduler;
    gTaskScheduler = nullptr;
}

inline TaskScheduler& getTaskScheduler() {
    if (!gTaskScheduler) {
        initTaskScheduler();
    }
    return *gTaskScheduler;
}

} // namespace frabgine

#endif // FRABGINE_CORE_UTILS_TASKSCHEDULER_HPP
