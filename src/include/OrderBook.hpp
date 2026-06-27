#pragma once
#include "LevelInfo.hpp"
#include "ModifyOrder.hpp"
#include "Order.hpp"
#include "OrderSide.hpp"
#include "TradeInfo.hpp"
#include "Usings.hpp"
#include "map"
#include "unordered_map"
#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <ctime>
#include <chrono>
#include <memory>
#include <boost/unordered/unordered_flat_map.hpp>
#include "CustomDLL.hpp"
#include "tsl/robin_map.h"
#include "absl/container/btree_map.h"

class OrderBook {
public:
  OrderBook();

  TradeInfos add_order(OrderPointer order);

  void cancel_order(OrderId);

  LevelsInfo get_order_book();

  std::size_t Size();
  
  TradeInfos modify_order(OrderModify modify_request);

  OrderPointer get_order_by_id(OrderId );
  
  ~OrderBook();
private:
  struct OrderInfoByID {
    OrderPointer pointer_; // points to the actual Order object
    OrderPointers::iterator
        it_; // this is the Order's iterator in a asks_ or bids_ list
  };

  enum class Action {
    Add,
    Remove,
    Match,
  };

  std::shared_ptr<MemoryPool<ListNode<OrderPointer>>> pool;

  std::map<Price, OrderPointers, std::greater<Price>> bids_;
  std::map<Price, OrderPointers, std::less<Price>> asks_;
  tsl::robin_map<OrderId, OrderInfoByID> orders_;
  LevelsInfo levels;
  mutable std::mutex ordersMutex_;
  std::thread ordersPruneThread_;
  std::condition_variable shutdownConditionVariable_;
  std::atomic<bool> shutdown_{ false };
  void PruneGoodForDayOrders();
  void OnOrderCancelled(Price price, Quantity quantity, OrderSide side);

  void OnOrderAdded(OrderPointer order);

  void OnOrderMatched(Price price, Quantity quantity, OrderSide side);

  void UpdateLevelData(Price price, Quantity quantity, OrderSide side,
                       Action action);

  bool can_match();
  bool can_match_order(OrderSide side, Price price);
  bool can_fully_match_order(OrderSide side, Price price, Quantity quantity);
  void cancel_orders_internal(OrderIds);
  void cancel_order_internal(OrderId, bool no_update_level = false);
  TradeInfos add_order_internal(OrderPointer order);
  TradeInfos match_orders();

};
