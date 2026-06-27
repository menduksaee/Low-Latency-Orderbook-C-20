
#include "include/OrderBook.hpp"
#include "include/Order.hpp"
#include "include/OrderSide.hpp"
#include "include/OrderType.hpp"
#include "include/Usings.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>


static inline uint64_t get_time_nanoseconds() {
  auto now = std::chrono::high_resolution_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

OrderBook::OrderBook()
    : ordersPruneThread_{[this]() { PruneGoodForDayOrders(); }}, pool(std::make_shared<MemoryPool<ListNode<OrderPointer>>>(3000000)) {
  std::cout << "Order Book Initialized, has 0 orders currently" << std::endl;
  // Pre-size for ~3,000,000 orders to avoid rehash spikes
  orders_.max_load_factor(0.7f);
  orders_.reserve(3000000);
}
void OrderBook::PruneGoodForDayOrders() {//have to test.

  using namespace std::chrono;
  const auto end = hours(16);

  while (true) {

    const auto now = system_clock::now();
    const auto now_c = system_clock::to_time_t(now);
    std::tm now_parts;
    localtime_r(&now_c, &now_parts);

    if (now_parts.tm_hour >= end.count())
      now_parts.tm_mday += 1;

    now_parts.tm_hour = end.count();
    now_parts.tm_min = 0;
    now_parts.tm_sec = 0;
    auto next = system_clock::from_time_t(mktime(&now_parts));
    auto till = next - now + milliseconds(100);

    {
      std::unique_lock ordersLock{ordersMutex_};
      if (shutdown_.load(std::memory_order_acquire) ||
          shutdownConditionVariable_.wait_for(ordersLock, till) ==
              std::cv_status::no_timeout)

        /*
          If shutdown_ atomic flag is set to true(during the destruction of the orderbook)
          or if the thread wakes up from the condition Variable due to notify or Spurious?
          Return from the function.

          Else timeout has occured, process end of day.
        */
        return;
    }

    OrderIds order_ids;

    {
      std::scoped_lock prunelock(ordersMutex_);
      for(const auto& [_,entry]:orders_){
          const auto & [orderptr , __] = entry;
          if(orderptr->get_order_type() == OrderType::GoodForDay){
            order_ids.push_back(orderptr->get_order_id());
          }
      }
    }

    cancel_orders_internal(order_ids);
  }
}

TradeInfos OrderBook::add_order (OrderPointer order) {
  std::scoped_lock ordersLock{ordersMutex_};
  return add_order_internal(order);
}

TradeInfos OrderBook::add_order_internal(OrderPointer order) {
  auto id = order->get_order_id();
  auto side = order->get_order_side();
  auto price = order->get_price();

  if (orders_.find(id) != orders_.end()) [[unlikely]] {
    return {};
  }

  if ((order->get_order_type() == OrderType::FillAndKill &&
      !can_match_order(side, price)))
    return {};

  if (order->get_order_type() == OrderType::FillOrKill &&
      !can_fully_match_order(side, price, order->get_quantity()))
    return {};

  if (order->get_order_type() == OrderType::Market) {
    order->market_normalize();
  }
  price = order->get_price();
  OrderPointers::iterator it;
  if (side == OrderSide::Buy) {
    auto it_level = bids_.find(price);
    if (it_level == bids_.end()) {
      it_level = bids_.emplace(price, OrderPointers(pool)).first;
    }
    auto &bids_list = it_level->second;
    it = bids_list.emplace_back(order);
  } else if (side == OrderSide::Sell) {
    auto it_level = asks_.find(price);
    if (it_level == asks_.end()) {
      it_level = asks_.emplace(price, OrderPointers(pool)).first;
    }
    auto &asks_list = it_level->second;
    it = asks_list.emplace_back(order);
  }
  orders_.try_emplace(id, OrderInfoByID{order, it});
  OnOrderAdded(order);

  const auto & trades_made=match_orders();
  return trades_made;
}

void OrderBook::cancel_order(OrderId id) {
  std::scoped_lock ordersLock{ordersMutex_};
  if (orders_.find(id) == orders_.end())
    return;
  cancel_order_internal(id);
}

LevelsInfo OrderBook::get_order_book() { return levels; }

std::size_t OrderBook::Size() { return orders_.size(); }

TradeInfos OrderBook::modify_order(OrderModify modify_request) {
  std::scoped_lock modifyorder(ordersMutex_);

  auto id = modify_request.get_order_id();
  if (orders_.find(id) == orders_.end()) {
    return {};
  }

  // Cancel the existing order
  cancel_order_internal(id);

  // Add the modified order
  return add_order_internal(modify_request.to_order_ptr());
}

void OrderBook::OnOrderCancelled(Price price, Quantity quantity, OrderSide side) {
  UpdateLevelData(price, quantity, side, Action::Remove);
}

void OrderBook::OnOrderAdded(OrderPointer order) {
  UpdateLevelData(order->get_price(), order->get_quantity(),
                  order->get_order_side(), Action::Add);
}

void OrderBook::OnOrderMatched(Price price, Quantity quantity, OrderSide side) {
  UpdateLevelData(price, quantity, side, Action::Match);
}

void OrderBook::UpdateLevelData(Price price, Quantity quantity, OrderSide side,
                                Action action) {
  auto &buy_side = levels.buy_levels_;
  auto &sell_side = levels.sell_levels_;
  //   auto &data_side = (side == OrderSide::Buy) ? *buy_side : *sell_side;
  // if (side == OrderSide::Buy and action!=Action::Add and buy_side.find(price) == buy_side.end()) {
  //   return;
  // }
  // if (side == OrderSide::Sell and action!=Action::Add and sell_side.find(price) == sell_side.end()) {
  //   return;
  // }
  auto &data = (side == OrderSide::Buy) ? buy_side[price] : sell_side[price];

  data.price_ = price;
  data.count_ += action == Action::Remove ? -1 : action == Action::Add ? 1 : 0;
  if (action == Action::Remove || action == Action::Match) {
    data.quantity_ -= quantity;
  } else {
    data.quantity_ += quantity;
  }
  if (side == OrderSide::Buy and data.count_ == 0) {
    buy_side.erase(price);
  }
  if (side == OrderSide::Sell and data.count_ == 0) {
    sell_side.erase(price);
  }

}

bool OrderBook::can_match() { // Returns 1 if OB is not saturated
  if (bids_.size() == 0 || asks_.size() == 0) {
    return false;
  }
  auto &[buy_price, _] = *bids_.begin();

  auto &[sell_price, __] = *asks_.begin();


  return (buy_price >= sell_price);
}

bool OrderBook::can_match_order(
    OrderSide side,
    Price price) { // Can a particular Order be matched- Used to
                   // check FillAndKIll orders before adding them
  if (side == OrderSide::Buy) {
    if (asks_.size() == 0)
      return false;
    auto &[sell_price, _] = *asks_.begin();
    if (sell_price <= price)
      return true;
    else
      return false;

  } else {
    if (bids_.size() == 0)
      return false;
    auto &[buy_price, _] = *bids_.begin();
    if (buy_price >= price)
      return true;
    else
      return false;
  }
}

bool OrderBook::can_fully_match_order(OrderSide side, Price price,
                                      Quantity quantity) {
  if (side == OrderSide::Buy) {
    // check in sell side levels if this much quantity can be filled.
    Quantity can_fill= 0;
    auto &sell_levels = levels.sell_levels_;

    for (auto [sell_price, levelinfo] : sell_levels) {
      if (sell_price > price)
        break;
      can_fill += levelinfo.quantity_;
      if (can_fill >= quantity)
        return true;
    }
    if (can_fill >= quantity)
      return true;
    else
      return false;
  } else if (side == OrderSide::Sell) {
    // check in buy side levels if this much quantity can be filled.
    Quantity can_fill= 0;
    auto &buy_levels = levels.buy_levels_;

    for (auto [buy_price, levelinfo] : buy_levels) {
      if (buy_price < price)
        break;
      can_fill += levelinfo.quantity_;
      if (can_fill >= quantity)
        return true;
    }
    if (can_fill >= quantity)
      return true;
    else
      return false;
  }

  else
    return false;
}

TradeInfos OrderBook::match_orders() {
  if (!can_match())
  return TradeInfos{};

TradeInfos trades_made;
trades_made.trades_made_.reserve(10);
while (true) {
    if (bids_.size() == 0 || asks_.size() == 0) {
        break;
      }
      
      auto &bids_pair = *bids_.begin();
      auto &asks_pair = *asks_.begin();
      auto &bids_list = bids_pair.second;
      auto &asks_list = asks_pair.second;
      auto bid_it = bids_list.begin();
      auto ask_it = asks_list.begin();
  
      auto &bid_order = **bid_it;
      auto &ask_order = **ask_it;
      
      if(bids_pair.first < asks_pair.first)
      break;
    
      Quantity trade_quantity =
      std::min(bid_order.get_quantity(), ask_order.get_quantity());
      
      Price trade_price = ask_order.get_price();
      if (ask_order.get_order_type() == OrderType::Market)
      trade_price = bid_order.get_price();
    
      auto buy_order_id = bid_order.get_order_id();
      auto sell_order_id = ask_order.get_order_id();
      trades_made.emplace_back(
        TradeInfo::SideInfoTrade{buy_order_id, bid_order.get_price()},
        TradeInfo::SideInfoTrade{sell_order_id, ask_order.get_price()},
        trade_price, trade_quantity);
      
      bid_order.fill_order(trade_quantity);
      if (bid_order.is_filled()) {
        // cancel_order_internal(buy_order_id, true);
        bids_list.erase(bid_it);
        if (bids_list.empty()) {
          bids_.erase(bids_pair.first);
        }
        OnOrderCancelled(bids_pair.first, trade_quantity, OrderSide::Buy);
        orders_.erase(buy_order_id);
      }
      else{// this won't bring down the level count.
        OnOrderMatched(bids_pair.first, trade_quantity, OrderSide::Buy);
        
      }
      
      ask_order.fill_order(trade_quantity);
      if (ask_order.is_filled()) {
        // cancel_order_internal(sell_order_id, true);
        asks_list.erase(ask_it);
        if (asks_list.empty()) {
          asks_.erase(asks_pair.first);
        }
        OnOrderCancelled(asks_pair.first, trade_quantity, OrderSide::Sell);
        orders_.erase(sell_order_id);
      }
      else{// this won't bring down the level count.
        OnOrderMatched(asks_pair.first, trade_quantity, OrderSide::Sell);
      }

  }
      if (bids_.size() > 0) {
        auto &[buy_price, bids_list] = *bids_.begin();
        auto &bids_entry = *bids_list.begin();
        auto buy_order_id = bids_entry->get_order_id();

    if (bids_entry->get_order_type() == OrderType::FillAndKill) {
      cancel_order_internal(buy_order_id);
    }
  }

  if (asks_.size() > 0) {
    auto &[sell_price, asks_list] = *asks_.begin();
    auto &asks_entry = *asks_list.begin();
    auto sell_order_id = asks_entry->get_order_id();

    if (asks_entry->get_order_type() == OrderType::FillAndKill) {
      cancel_order_internal(sell_order_id);
    }
  }

  return trades_made;
}

OrderBook::~OrderBook() {
  shutdown_.store(true, std::memory_order_release);
  shutdownConditionVariable_.notify_one();
  ordersPruneThread_.join();
}

void OrderBook::cancel_order_internal(OrderId id, bool no_update_level) {


  auto &orderinfo = orders_[id];
  OrderPointers::iterator it = orderinfo.it_;
  auto &order = orderinfo.pointer_;
  auto price = order->get_price();
  auto quantity = order->get_quantity();
  auto side = order->get_order_side();

  if (side == OrderSide::Buy) {
    auto &bids_list = bids_[price];
    bids_list.erase(it);
    if (bids_list.empty()) {
      bids_.erase(price);
    }
  } else if (side == OrderSide::Sell) {
    auto &asks_list = asks_[price];
    asks_list.erase(it);
    if (asks_list.empty()) {
      asks_.erase(price);
    }
  }
  if(!no_update_level)OnOrderCancelled(price, quantity, side);
  orders_.erase(id);
}

void OrderBook::cancel_orders_internal(OrderIds ids){
  std::scoped_lock cancel_lock(ordersMutex_);
  for(auto order_id: ids){
    cancel_order_internal(order_id);
  }

}

OrderPointer OrderBook::get_order_by_id(OrderId id){

  return orders_[id].pointer_;
}
