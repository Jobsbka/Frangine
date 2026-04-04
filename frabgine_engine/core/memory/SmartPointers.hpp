#ifndef FRABGINE_CORE_MEMORY_SMARTPOINTERS_HPP
#define FRABGINE_CORE_MEMORY_SMARTPOINTERS_HPP

#include <memory>
#include <vector>
#include <unordered_map>

namespace frabgine {

// Основные типы умных указателей на базе STL
template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T>
using WeakPtr = std::weak_ptr<T>;

// Фабричные функции для создания умных указателей
template<typename T, typename... Args>
UniquePtr<T> makeUnique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

// Менеджер ресурсов с автоматическим управлением временем жизни
template<typename T>
class ResourceHandle {
private:
    SharedPtr<T> resource_;
    
public:
    ResourceHandle() = default;
    
    explicit ResourceHandle(SharedPtr<T> resource) 
        : resource_(std::move(resource)) {}
    
    T* get() const { return resource_.get(); }
    T& operator*() const { return *resource_; }
    T* operator->() const { return resource_.get(); }
    
    explicit operator bool() const { 
        return resource_ != nullptr; 
    }
    
    long useCount() const { 
        return resource_.use_count(); 
    }
};

// Базовый класс для объектов с подсчетом ссылок
class RefCounted {
private:
    mutable int refCount_ = 0;
    
public:
    virtual ~RefCounted() = default;
    
    void addRef() const {
        ++refCount_;
    }
    
    void release() const {
        if (--refCount_ == 0) {
            delete this;
        }
    }
    
    int refCount() const {
        return refCount_;
    }
};

// Умный указатель для объектов с ручным подсчетом ссылок
template<typename T>
class RefPtr {
private:
    T* ptr_ = nullptr;
    
public:
    RefPtr() = default;
    
    explicit RefPtr(T* ptr) : ptr_(ptr) {
        if (ptr_) ptr_->addRef();
    }
    
    RefPtr(const RefPtr& other) : ptr_(other.ptr_) {
        if (ptr_) ptr_->addRef();
    }
    
    RefPtr& operator=(const RefPtr& other) {
        if (this != &other) {
            if (ptr_) ptr_->release();
            ptr_ = other.ptr_;
            if (ptr_) ptr_->addRef();
        }
        return *this;
    }
    
    ~RefPtr() {
        if (ptr_) ptr_->release();
    }
    
    T* get() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    
    explicit operator bool() const { 
        return ptr_ != nullptr; 
    }
    
    void reset(T* ptr = nullptr) {
        if (ptr_) ptr_->release();
        ptr_ = ptr;
        if (ptr_) ptr_->addRef();
    }
};

} // namespace frabgine

#endif // FRABGINE_CORE_MEMORY_SMARTPOINTERS_HPP
