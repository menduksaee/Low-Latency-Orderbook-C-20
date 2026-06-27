#pragma once
#include <vector>
#include <cstddef>
#include <cassert>
#include <algorithm>
#include <memory>
#include <new>
#include <utility>

// Custom Memory Pool for template types using an intrusive free list
template<typename T>
class MemoryPool {
private:
    struct FreeNode { FreeNode* next; };

    struct Chunk {
        std::unique_ptr<std::byte[]> buffer;
        size_t capacity;   // number of T slots
        size_t used;       // bump index within this chunk

        explicit Chunk(size_t slots)
            : buffer(new std::byte[slots * sizeof(T)]), capacity(slots), used(0) {}

        T* allocate_from_chunk() {
            if (used >= capacity) return nullptr;
            T* ptr = reinterpret_cast<T*>(buffer.get()) + used;
            used += 1;
            return ptr;
        }
    };

    std::vector<std::unique_ptr<Chunk>> chunks_;
    FreeNode* free_head_ = nullptr;     // head of free list (intrusive)
    size_t next_chunk_slots_;

    T* acquire_raw_slot() {
        if (free_head_ != nullptr) {
            T* node = reinterpret_cast<T*>(free_head_);
            free_head_ = free_head_->next;
            return node;
        }
        T* node = chunks_.back()->allocate_from_chunk();
        if (node == nullptr) {
            // grow moderately (1.5x)
            size_t grown = next_chunk_slots_ + next_chunk_slots_ / 2;
            next_chunk_slots_ = grown > 0 ? grown : 1;
            chunks_.emplace_back(std::make_unique<Chunk>(next_chunk_slots_));
            node = chunks_.back()->allocate_from_chunk();
        }
        return node;
    }

public:
    explicit MemoryPool(size_t initial_slots = 100000)
        : next_chunk_slots_(std::max<size_t>(1024, initial_slots)) {
        chunks_.emplace_back(std::make_unique<Chunk>(next_chunk_slots_));
    }

    // Pre-allocate at least 'slots' total capacity
    void reserve_slots(size_t slots) {
        while (total_capacity() < slots) {
            size_t grown = next_chunk_slots_ + next_chunk_slots_ / 2;
            next_chunk_slots_ = grown > 0 ? grown : 1;
            chunks_.emplace_back(std::make_unique<Chunk>(next_chunk_slots_));
        }
    }

    T* allocate() {
        T* node = acquire_raw_slot();
        ::new (static_cast<void*>(node)) T();
        return node;
    }

    template <typename... Args>
    T* allocate_emplace(Args&&... args) {
        T* node = acquire_raw_slot();
        ::new (static_cast<void*>(node)) T(std::forward<Args>(args)...);
        return node;
    }

    void deallocate(T* ptr) {
        if (ptr == nullptr) return;
        // Call destructor, then push onto the free list using node storage itself
        ptr->~T();
        FreeNode* f = reinterpret_cast<FreeNode*>(ptr);
        f->next = free_head_;
        free_head_ = f;
    }

    size_t total_capacity() const {
        size_t total = 0;
        for (const auto& c : chunks_) total += c->capacity;
        return total;
    }

    size_t chunk_count() const { return chunks_.size(); }
};