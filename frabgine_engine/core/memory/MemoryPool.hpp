#ifndef FRABGINE_CORE_MEMORY_MEMORYPOOL_HPP
#define FRABGINE_CORE_MEMORY_MEMORYPOOL_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <stdexcept>

namespace frabgine {

// Пул памяти для эффективного выделения объектов одного типа
template<typename T, size_t BlockSize = 256>
class MemoryPool {
private:
    struct Block {
        std::unique_ptr<uint8_t[]> data;
        size_t index;
        
        Block() : data(std::make_unique<uint8_t[]>(BlockSize * sizeof(T))), index(0) {}
    };
    
    std::vector<Block> blocks_;
    std::vector<void*> freeList_;
    
public:
    MemoryPool() {
        blocks_.emplace_back();
    }
    
    ~MemoryPool() {
        // Вызов деструкторов для всех выделенных объектов
        for (void* ptr : freeList_) {
            if (ptr) {
                static_cast<T*>(ptr)->~T();
            }
        }
    }
    
    template<typename... Args>
    T* allocate(Args&&... args) {
        void* ptr = nullptr;
        
        if (!freeList_.empty()) {
            ptr = freeList_.back();
            freeList_.pop_back();
        } else {
            Block& currentBlock = blocks_.back();
            
            if (currentBlock.index >= BlockSize) {
                blocks_.emplace_back();
                currentBlock = blocks_.back();
            }
            
            ptr = currentBlock.data.get() + (currentBlock.index++ * sizeof(T));
        }
        
        return new (ptr) T(std::forward<Args>(args)...);
    }
    
    void deallocate(T* ptr) {
        if (!ptr) return;
        
        // Вызов деструктора
        ptr->~T();
        
        // Добавление в свободный список для повторного использования
        freeList_.push_back(ptr);
    }
    
    void clear() {
        freeList_.clear();
        blocks_.clear();
        blocks_.emplace_back();
    }
    
    size_t allocatedCount() const {
        size_t total = 0;
        for (const auto& block : blocks_) {
            total += block.index;
        }
        return total - freeList_.size();
    }
};

// Базовый класс для системного аллокатора
class SystemAllocator {
public:
    virtual ~SystemAllocator() = default;
    
    virtual void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) = 0;
    virtual void deallocate(void* ptr) = 0;
    virtual size_t getAllocatedSize() const = 0;
};

// Простой аллокатор на базе malloc/free
class DefaultAllocator : public SystemAllocator {
private:
    size_t allocatedSize_ = 0;
    
public:
    void* allocate(size_t size, size_t alignment) override {
        void* ptr = aligned_alloc(alignment, size);
        if (ptr) {
            allocatedSize_ += size;
        }
        return ptr;
    }
    
    void deallocate(void* ptr) override {
        if (ptr) {
            free(ptr);
        }
    }
    
    size_t getAllocatedSize() const override {
        return allocatedSize_;
    }
};

// Глобальный экземпляр аллокатора по умолчанию
inline DefaultAllocator gDefaultAllocator;

// Перегруженные операторы new/delete для классов
template<typename T>
void* operator new(size_t size, T& allocator) {
    return allocator.allocate(size);
}

template<typename T>
void operator delete(void* ptr, T& allocator) {
    allocator.deallocate(ptr);
}

} // namespace frabgine

#endif // FRABGINE_CORE_MEMORY_MEMORYPOOL_HPP
