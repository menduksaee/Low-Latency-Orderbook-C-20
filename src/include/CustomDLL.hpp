#pragma once
#include "MemoryPool.hpp"
#include <cstddef>
#include <utility>
#include <memory>


// Node structure for the linked list
template<typename T>
struct ListNode {
    T data;
    ListNode* next;
    ListNode* prev;

    ListNode() : data(), next(nullptr), prev(nullptr) {}
    ListNode(const T& value) : data(value), next(nullptr), prev(nullptr) {}
    template <typename... Args>
    explicit ListNode(Args&&... args) : data(std::forward<Args>(args)...), next(nullptr), prev(nullptr) {}
};

// Custom Linked List using the memory pool
template<typename T>
class CustomLinkedList {
private:
    // Shared ownership pool; by default a unique pool per list instance
    std::shared_ptr< MemoryPool<ListNode<T>> > pool_;
    ListNode<T>* head_;
    ListNode<T>* tail_;
    size_t size_;

public:
    // Default constructor: create a unique pool for this list (no implicit sharing)
    CustomLinkedList()
        : pool_(std::make_shared< MemoryPool<ListNode<T>> >()), head_(nullptr), tail_(nullptr), size_(0) {
            // std::cout<<"DLL 1"<<std::endl;

        }

    // Inject external shared pool to share across many lists of same T
    explicit CustomLinkedList(std::shared_ptr< MemoryPool<ListNode<T>> > pool)
        : pool_(std::move(pool)), head_(nullptr), tail_(nullptr), size_(0) {
            // std::cout<<"DLL 2"<<std::endl;
        }

    // Optional: accept non-owning reference; wrap with no-op deleter
    explicit CustomLinkedList(MemoryPool<ListNode<T>>& pool)
        : pool_(std::shared_ptr< MemoryPool<ListNode<T>> >(&pool, [](MemoryPool<ListNode<T>>*){})), head_(nullptr), tail_(nullptr), size_(0) {
            // std::cout<<"DLL 3"<<std::endl;
        }

    ~CustomLinkedList() { clear(); }

    auto push_back(const T& value) {
        return emplace_back(value);
    }

    template <typename... Args>
    auto emplace_back(Args&&... args) {
        ListNode<T>* new_node = pool_->allocate_emplace(std::forward<Args>(args)...);
        if (new_node) {
            new_node->next = nullptr;
            new_node->prev = tail_;
            if (!tail_) {
                head_ = tail_ = new_node;
            } else {
                tail_->next = new_node;
                tail_ = new_node;
            }
            size_++;
        }
        return iterator(new_node);
    }

    auto push_front(const T& value) {
        return emplace_front(value);
    }

    template <typename... Args>
    auto emplace_front(Args&&... args) {
        ListNode<T>* new_node = pool_->allocate_emplace(std::forward<Args>(args)...);
        if (new_node) {
            new_node->prev = nullptr;
            new_node->next = head_;
            if (head_) head_->prev = new_node;
            head_ = new_node;
            if (!tail_) tail_ = head_;
            size_++;
        }
        return iterator(new_node);
    }

    void pop_front() {
        if (head_) {
            ListNode<T>* old_head = head_;
            head_ = head_->next;
            if (head_) head_->prev = nullptr;
            if (!head_) tail_ = nullptr;
            pool_->deallocate(old_head);
            size_--;
        }
    }

    void pop_back() {
        if (tail_) {
            ListNode<T>* old_tail = tail_;
            tail_ = tail_->prev;
            if (tail_) tail_->next = nullptr;
            if (!tail_) head_ = nullptr;
            pool_->deallocate(old_tail);
            size_--;
        }
    }

    void clear() {
        while (head_) {
            ListNode<T>* temp = head_;
            head_ = head_->next;
            pool_->deallocate(temp);
        }
        head_ = tail_ = nullptr;
        size_ = 0;
    }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    // iterator support
    class iterator {
    private:
        ListNode<T>* current_;
    public:
        explicit iterator(ListNode<T>* node) : current_(node) {}
        explicit iterator():current_(nullptr) {}
        T& operator*() { return current_->data; }
        const T& operator*() const { return current_->data; }
        iterator& operator++() { current_ = current_->next; return *this; }
        bool operator!=(const iterator& other) const { return current_ != other.current_; }
        // Access to underlying node for erase operations
        ListNode<T>* get_node() const { return current_; }
    };

    iterator begin() { return iterator(head_); }
    iterator end() { return iterator(nullptr); }

    // Erase element at iterator; returns iterator to next element
    iterator erase(iterator pos) {
        ListNode<T>* node = pos.get_node();
        if (!node) return end();
        ListNode<T>* next = node->next;

        if (node->prev) node->prev->next = node->next; else head_ = node->next;
        if (node->next) node->next->prev = node->prev; else tail_ = node->prev;

        pool_->deallocate(node);
        size_--;
        return iterator(next);
    }

    // Pool stats (from the current pool)
    size_t pool_capacity() const { return pool_->total_capacity(); }
    size_t pool_chunks() const { return pool_->chunk_count(); }
};