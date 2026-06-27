#pragma once
#include "MemoryPool.hpp"
#include <memory>
#include <utility>
#include <boost/intrusive_ptr.hpp>
#include "Order.hpp"
// Deleter for shared_ptr (kept for possible reuse)
template <typename T>
struct PooledDeleter {
	std::shared_ptr< MemoryPool<T> > pool;
	void operator()(T* ptr) const noexcept {
		if (ptr) {
			pool->deallocate(ptr);
		}
	}
};

// Create a shared_ptr whose storage comes from MemoryPool<T>
// Pool is held by shared_ptr in deleter to keep it alive as long as any object exists
template <typename T, typename... Args>
std::shared_ptr<T> make_shared_pooled(const std::shared_ptr< MemoryPool<T> >& pool, Args&&... args) {
	T* raw = pool->allocate_emplace(std::forward<Args>(args)...);
	return std::shared_ptr<T>(raw, PooledDeleter<T>{pool});
}

// Create an intrusive_ptr<Order> from a MemoryPool<Order>
inline boost::intrusive_ptr<Order> make_intrusive_pooled_order(MemoryPool<Order>* pool,
                                                              OrderType type_ip, OrderSide side, OrderId id, Price price, Quantity qty) {
	Order* raw = pool->allocate_emplace(pool, type_ip, side, id, price, qty);
	// adopt with add_ref=true so refcount starts at 1
	return boost::intrusive_ptr<Order>(raw, true);
}
