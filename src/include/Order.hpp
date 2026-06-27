#pragma once
#include "Usings.hpp"
#include "OrderType.hpp"
#include "OrderSide.hpp"
#include<memory>
#include <list>
#include <stdexcept>
#include <atomic>
#include <boost/intrusive_ptr.hpp>
#include "CustomDLL.hpp"

// Forward declare MemoryPool
template <typename T>
class MemoryPool;

class Order{

    public:
        // Pool-aware constructor
        Order(MemoryPool<Order>* pool_ptr,
              OrderType type_ip, OrderSide side, OrderId id, Price price, Quantity quantity)
        : type_(type_ip),
          side_(side),id_(id), price_(price), quantity_order_(quantity), quantity_order_left_(quantity), pool_(pool_ptr) {}

        // Legacy constructor (no pool)
        Order(OrderType type_ip, OrderSide side, OrderId id, Price price, Quantity quantity)
        : type_(type_ip),
          side_(side),id_(id), price_(price), quantity_order_(quantity), quantity_order_left_(quantity), pool_(nullptr) {}

        bool is_filled(){
            return quantity_order_left_ == 0;
        }

        OrderType  get_order_type(){
            return type_;
        }

        OrderSide get_order_side(){
            return side_;
        }

        Quantity get_quantity(){
            return quantity_order_left_;
        }

        Price get_price(){
             return price_;
         }
         
         OrderId get_order_id(){
             return id_;
         }
         
         void fill_order(Quantity quantity){
             quantity_order_left_ -= quantity;
         }

         bool order_filled_partial_or_full(){
            return quantity_order_left_ < quantity_order_;
         }
         void market_normalize(){
            if(get_order_type() != OrderType::Market){
                throw std::runtime_error("Order type is not Market in market_to_gtc()");
            }
            // type_ = OrderType::GoodTillCancel;
            price_ = (get_order_side()==OrderSide::Buy)? 1e18:0;

         }

         // Set pool pointer if constructed without one
         void attach_pool(MemoryPool<Order>* pool_ptr){ pool_ = pool_ptr; }

         // Intrusive pointer hooks
         friend inline void intrusive_ptr_add_ref(Order* p) {
             p->ref_count_.fetch_add(1, std::memory_order_relaxed);
         }
         friend inline void intrusive_ptr_release(Order* p) {
             if (p->ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                 if (p->pool_ != nullptr) {
                     // MemoryPool::deallocate will run the destructor
                     p->pool_->deallocate(p);
                 } else {
                     // Fallback if not pool-backed
                     delete p;
                 }
             }
         }

    private:

    OrderType type_;
    OrderSide side_;
    OrderId id_;
    mutable Price price_;
    Quantity quantity_order_;
    Quantity quantity_order_left_;
    mutable std::atomic<uint32_t> ref_count_{0};
    MemoryPool<Order>* pool_;
};


using OrderPointer = boost::intrusive_ptr<Order> ;
using OrderPointers = CustomLinkedList<OrderPointer> ;
using OrderList = std::pmr::list<OrderPointer>;