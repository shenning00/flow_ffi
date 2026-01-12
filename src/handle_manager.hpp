#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <typeinfo>
#include <unordered_map>

namespace flow_ffi {

// Base class for all managed handles
class HandleBase {
public:
    HandleBase() : ref_count_(1) {}
    virtual ~HandleBase() = default;

    void retain() { ref_count_.fetch_add(1, std::memory_order_relaxed); }

    bool release() {
        int old_count = ref_count_.fetch_sub(1, std::memory_order_acq_rel);
        return old_count == 1; // Returns true if this was the last reference
    }

    int32_t get_ref_count() const { return ref_count_.load(std::memory_order_acquire); }

    virtual const char* get_type_name() const = 0;

private:
    std::atomic<int32_t> ref_count_;
};

// Template wrapper for specific handle types
template <typename T>
class Handle : public HandleBase {
public:
    template <typename... Args>
    Handle(Args&&... args) : object_(std::forward<Args>(args)...) {}

    T& get() { return object_; }
    const T& get() const { return object_; }

    const char* get_type_name() const override { return typeid(T).name(); }

private:
    T object_;
};

// Thread-safe handle registry
class HandleRegistry {
public:
    static HandleRegistry& instance() {
        static HandleRegistry registry;
        return registry;
    }

    // Register a new handle
    void* register_handle(std::unique_ptr<HandleBase> handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        void* ptr = handle.get();
        handles_[ptr] = std::move(handle);
        return ptr;
    }

    // Get a handle (returns nullptr if not found or wrong type)
    template <typename T>
    T* get_handle(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handles_.find(ptr);
        if (it == handles_.end()) {
            return nullptr;
        }

        auto* handle = dynamic_cast<Handle<T>*>(it->second.get());
        if (!handle) {
            return nullptr;
        }

        return &handle->get();
    }

    // Get raw handle base (for reference counting)
    HandleBase* get_handle_base(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handles_.find(ptr);
        return (it != handles_.end()) ? it->second.get() : nullptr;
    }

    // Check if handle exists
    bool is_valid_handle(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        return handles_.find(ptr) != handles_.end();
    }

    // Remove handle (called when reference count reaches 0)
    void unregister_handle(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        handles_.erase(ptr);
    }

    // Get handle count (for debugging/testing)
    size_t get_handle_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return handles_.size();
    }

    // Clear all handles (for testing)
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        handles_.clear();
    }

private:
    HandleRegistry() = default;
    ~HandleRegistry() = default;

    mutable std::mutex mutex_;
    std::unordered_map<void*, std::unique_ptr<HandleBase>> handles_;
};

// Helper functions for creating and managing handles

template <typename T, typename... Args>
void* create_handle(Args&&... args) {
    auto handle = std::make_unique<Handle<T>>(std::forward<Args>(args)...);
    return HandleRegistry::instance().register_handle(std::move(handle));
}

template <typename T>
T* get_handle(void* ptr) {
    return HandleRegistry::instance().get_handle<T>(ptr);
}

inline bool is_valid_handle(void* ptr) {
    return HandleRegistry::instance().is_valid_handle(ptr);
}

inline void retain_handle(void* ptr) {
    if (auto* handle = HandleRegistry::instance().get_handle_base(ptr)) {
        handle->retain();
    }
}

inline bool release_handle(void* ptr) {
    if (auto* handle = HandleRegistry::instance().get_handle_base(ptr)) {
        if (handle->release()) {
            HandleRegistry::instance().unregister_handle(ptr);
            return true;
        }
    }
    return false;
}

inline int32_t get_ref_count(void* ptr) {
    if (auto* handle = HandleRegistry::instance().get_handle_base(ptr)) {
        return handle->get_ref_count();
    }
    return 0;
}

} // namespace flow_ffi