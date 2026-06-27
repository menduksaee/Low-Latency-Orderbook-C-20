#include "include/OrderBook.hpp"
#include "include/PooledShared.hpp"
#include "include/MemoryPool.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <charconv>
#include <algorithm>

// ANSI Color codes
namespace Colors {
    const std::string RESET = "\033[0m";
    const std::string BOLD = "\033[1m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    const std::string BG_RED = "\033[41m";
    const std::string BG_GREEN = "\033[42m";
    const std::string BG_BLUE = "\033[44m";
}

enum class ActionType
{
    Add,
    Cancel,
    Modify,
    Show,
    Help,
    Quit,
    Invalid
};

struct OrderAction
{
    ActionType type_;
    OrderType orderType_;
    OrderSide side_;
    Price price_;
    Quantity quantity_;
    OrderId orderId_;
};

class OrderBookApp
{
private:
    MemoryPool<Order> order_pool_;  // Must be declared FIRST so it's destroyed LAST
    OrderBook orderbook_;
    OrderId next_order_id_;
    
    // Helper function to get current time in nanoseconds (monotonic) - for core OrderBook timing
    uint64_t get_time_nanoseconds() const {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    }
    
    std::uint32_t ToNumber(const std::string_view& str) const
    {
        std::int64_t value{};
        auto result = std::from_chars(str.data(), str.data() + str.size(), value);
        if (result.ec != std::errc{} || value < 0)
            throw std::invalid_argument("Invalid number: " + std::string(str));
        return static_cast<std::uint32_t>(value);
    }

    double ToPrice(const std::string_view& str) const
    {
        double value{};
        auto result = std::from_chars(str.data(), str.data() + str.size(), value);
        if (result.ec != std::errc{} || value < 0.0)
            throw std::invalid_argument("Invalid price: " + std::string(str));
        return value;
    }

    std::vector<std::string> Split(const std::string& str, char delimiter = ' ') const
    {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;
        while (std::getline(ss, token, delimiter))
        {
            if (!token.empty())
                tokens.push_back(token);
        }
        return tokens;
    }

    OrderSide ParseSide(const std::string& str) const
    {
        if (str == "B" || str == "Buy" || str == "buy")
            return OrderSide::Buy;
        else if (str == "S" || str == "Sell" || str == "sell")
            return OrderSide::Sell;
        else
            throw std::invalid_argument("Unknown OrderSide: " + str);
    }

    OrderType ParseOrderType(const std::string& str) const
    {
        if (str == "FillAndKill" || str == "FAK")
            return OrderType::FillAndKill;
        else if (str == "GoodTillCancel" || str == "GTC")
            return OrderType::GoodTillCancel;
        else if (str == "GoodForDay" || str == "GFD")
            return OrderType::GoodForDay;
        else if (str == "FillOrKill" || str == "FOK")
            return OrderType::FillOrKill;
        else if (str == "Market" || str == "MKT")
            return OrderType::Market;
        else
            throw std::invalid_argument("Unknown OrderType: " + str);
    }

    OrderAction ParseCommand(const std::string& input)
    {
        OrderAction action{};
        auto tokens = Split(input);
        
        if (tokens.empty())
        {
            action.type_ = ActionType::Invalid;
            return action;
        }

        std::string command = tokens[0];
        std::transform(command.begin(), command.end(), command.begin(), ::tolower);

        if (command == "a" || command == "add")
        {
            // A <side> <orderType> <price> <quantity> [orderId] - orderId is optional, will use next_order_id_ if not provided
            if (tokens.size() < 5 || tokens.size() > 6)
                throw std::invalid_argument("Add command requires: A <side> <orderType> <price> <quantity> [orderId]");
            
            action.type_ = ActionType::Add;
            action.side_ = ParseSide(tokens[1]);
            action.orderType_ = ParseOrderType(tokens[2]);
            action.price_ = ToPrice(tokens[3]);
            action.quantity_ = ToNumber(tokens[4]);
            if (tokens.size() == 6) {
                action.orderId_ = ToNumber(tokens[5]);
                // Update next_order_id_ to be higher than manually specified ID
                if (action.orderId_ >= next_order_id_) {
                    next_order_id_ = action.orderId_ + 1;
                }
            } else {
                action.orderId_ = next_order_id_++;
            }
        }
        else if (command == "c" || command == "cancel")
        {
            // C <orderId>
            if (tokens.size() != 2)
                throw std::invalid_argument("Cancel command requires: C <orderId>");
            
            action.type_ = ActionType::Cancel;
            action.orderId_ = ToNumber(tokens[1]);
        }
        else if (command == "m" || command == "modify")
        {
            // M <orderId> <side> <price> <quantity>
            if (tokens.size() != 5)
                throw std::invalid_argument("Modify command requires: M <orderId> <side> <price> <quantity>");
            
            action.type_ = ActionType::Modify;
            action.orderId_ = ToNumber(tokens[1]);
            action.side_ = ParseSide(tokens[2]);
            action.price_ = ToPrice(tokens[3]);
            action.quantity_ = ToNumber(tokens[4]);
        }
        else if (command == "s" || command == "show")
        {
            action.type_ = ActionType::Show;
        }
        else if (command == "h" || command == "help")
        {
            action.type_ = ActionType::Help;
        }
        else if (command == "q" || command == "quit" || command == "exit")
        {
            action.type_ = ActionType::Quit;
        }
        else
        {
            action.type_ = ActionType::Invalid;
        }

        return action;
    }

    void ShowHelp() const
    {
        std::cout << Colors::CYAN << Colors::BOLD << "\n=== Order Book Commands ===\n" << Colors::RESET;
        std::cout << Colors::GREEN << "A <side> <orderType> <price> <quantity> [orderId]" << Colors::RESET << " - Add order (orderId optional)\n";
        std::cout << Colors::RED << "C <orderId>" << Colors::RESET << "                                       - Cancel order\n";
        std::cout << Colors::YELLOW << "M <orderId> <side> <price> <quantity>" << Colors::RESET << "             - Modify order\n";
        std::cout << Colors::BLUE << "S" << Colors::RESET << "                                                 - Show order book\n";
        std::cout << Colors::MAGENTA << "H" << Colors::RESET << "                                                 - Show help\n";
        std::cout << Colors::WHITE << "Q" << Colors::RESET << "                                                 - Quit\n\n";
        std::cout << Colors::BOLD << "Sides:" << Colors::RESET << " " << Colors::GREEN << "B" << Colors::RESET << "/Buy, " << Colors::RED << "S" << Colors::RESET << "/Sell\n";
        std::cout << Colors::BOLD << "Order Types:" << Colors::RESET << " GTC/GoodTillCancel, GFD/GoodForDay, FAK/FillAndKill, FOK/FillOrKill, MKT/Market\n";
        std::cout << Colors::BOLD << "Examples:\n" << Colors::RESET;
        std::cout << "  " << Colors::GREEN << "A B GTC 100.50 50" << Colors::RESET << "      - Add buy order at price 100.50, quantity 50 (auto ID)\n";
        std::cout << "  " << Colors::GREEN << "A S MKT 0 25 1002" << Colors::RESET << "      - Add market sell order, quantity 25, ID 1002\n";
        std::cout << "  " << Colors::RED << "C 1001" << Colors::RESET << "                 - Cancel order ID 1001\n";
        std::cout << "  " << Colors::YELLOW << "M 1001 B 105.25 60" << Colors::RESET << "     - Modify order 1001 to buy at 105.25 with quantity 60\n\n";
        std::cout << Colors::BOLD << "Next Available ID: " << Colors::CYAN << next_order_id_ << Colors::RESET << "\n\n";
    }

    void ShowOrderBook()
    {
        auto levels = orderbook_.get_order_book();
        auto& bids = levels.get_bids();
        auto& asks = levels.get_asks();

        std::cout << Colors::CYAN << Colors::BOLD << "\n=== Order Book Status ===" << Colors::RESET << "\n";
        std::cout << Colors::BOLD << "Total Orders: " << Colors::YELLOW << orderbook_.Size() << Colors::RESET << "\n";
        std::cout << Colors::BOLD << "Bid Levels: " << Colors::GREEN << bids.size() << Colors::RESET 
                  << Colors::BOLD << ", Ask Levels: " << Colors::RED << asks.size() << Colors::RESET << "\n";
        std::cout << Colors::BOLD << "Next Available ID: " << Colors::CYAN << next_order_id_ << Colors::RESET << "\n\n";

        std::cout << Colors::GREEN << Colors::BOLD << std::setw(14) << "BIDS" << Colors::RESET 
                  << " | " << Colors::RED << Colors::BOLD << std::setw(14) << "ASKS" << Colors::RESET << "\n";
        std::cout << Colors::GREEN << std::setw(8) << "Price" << " " << std::setw(5) << "Qty" << Colors::RESET 
                  << " | " << Colors::RED << std::setw(8) << "Price" << " " << std::setw(5) << "Qty" << Colors::RESET << "\n";
        std::cout << Colors::WHITE << "------------------------------" << Colors::RESET << "\n";

        // Convert to vectors for easier display
        std::vector<LevelInfo> bid_levels, ask_levels;
        for (const auto& [price, info] : bids)
            bid_levels.push_back(info);
        for (const auto& [price, info] : asks)
            ask_levels.push_back(info);

        // Sort bids descending, asks ascending
        std::sort(bid_levels.begin(), bid_levels.end(), 
                  [](const LevelInfo& a, const LevelInfo& b) { return a.price_ > b.price_; });
        std::sort(ask_levels.begin(), ask_levels.end(), 
                  [](const LevelInfo& a, const LevelInfo& b) { return a.price_ < b.price_; });

        size_t max_levels = std::max(bid_levels.size(), ask_levels.size());
        for (size_t i = 0; i < max_levels; ++i)
        {
            if (i < bid_levels.size())
                std::cout << Colors::GREEN << std::setw(8) << std::fixed << std::setprecision(2) << bid_levels[i].price_ << " " << std::setw(5) << bid_levels[i].quantity_ << Colors::RESET;
            else
                std::cout << std::setw(14) << "";
            
            std::cout << " | ";
            
            if (i < ask_levels.size())
                std::cout << Colors::RED << std::setw(8) << std::fixed << std::setprecision(2) << ask_levels[i].price_ << " " << std::setw(5) << ask_levels[i].quantity_ << Colors::RESET;
            else
                std::cout << std::setw(14) << "";
            
            std::cout << "\n";
        }
        std::cout << "\n";
    }

    void ShowTrades(TradeInfos& trades) const
    {
        if (trades.trades_made_.empty())
        {
            std::cout << Colors::YELLOW << "No trades executed." << Colors::RESET << "\n";
            return;
        }

        std::cout << Colors::MAGENTA << Colors::BOLD << "Trades executed (" << trades.trades_made_.size() << "):" << Colors::RESET << "\n";
        for (auto& trade : trades.trades_made_)
        {
            std::cout << Colors::CYAN;
            trade.print();
            std::cout << Colors::RESET;
        }
    }

    OrderPointer CreateOrder(const OrderAction& action)
    {
        return make_intrusive_pooled_order(
            &order_pool_,
            action.orderType_,
            action.side_,
            action.orderId_,
            action.price_,
            action.quantity_);
    }

    OrderModify CreateOrderModify(const OrderAction& action)
    {
        auto existing_order = orderbook_.get_order_by_id(action.orderId_);
        if (!existing_order)
            throw std::invalid_argument("Order ID " + std::to_string(action.orderId_) + " not found");
        
        return OrderModify(
            &order_pool_,
            existing_order->get_order_type(),
            action.side_,
            action.orderId_,
            action.price_,
            action.quantity_);
    }

public:
    OrderBookApp() : order_pool_(100000), next_order_id_(1000)
    {
        std::cout << Colors::CYAN << Colors::BOLD << "=== Order Book Application ===" << Colors::RESET << "\n";
        std::cout << Colors::YELLOW << "Type 'help' or 'h' for commands." << Colors::RESET << "\n";
        std::cout << Colors::BOLD << "Next Available ID: " << Colors::CYAN << next_order_id_ << Colors::RESET << "\n\n";
    }

    void Run()
    {
        std::string input;
        
        while (true)
        {
            std::cout << Colors::BOLD << "OrderBook[" << Colors::CYAN << next_order_id_ << Colors::RESET << Colors::BOLD << "]> " << Colors::RESET;
            std::getline(std::cin, input);
            
            if (input.empty())
                continue;

            try
            {
                OrderAction action = ParseCommand(input);
                TradeInfos trades;
                uint64_t core_orderbook_time = 0;
                
                switch (action.type_)
                {
                case ActionType::Add:
                {
                    auto order = CreateOrder(action);
                    // Time only the core OrderBook operation
                    uint64_t start_time = get_time_nanoseconds();
                    trades = orderbook_.add_order(order);
                    uint64_t end_time = get_time_nanoseconds();
                    core_orderbook_time = end_time - start_time;
                    
                    std::cout << Colors::GREEN << "✓ Order " << action.orderId_ << " added successfully." << Colors::RESET << "\n";
                    ShowTrades(trades);
                    break;
                }
                case ActionType::Cancel:
                {
                    // Time only the core OrderBook operation
                    uint64_t start_time = get_time_nanoseconds();
                    orderbook_.cancel_order(action.orderId_);
                    uint64_t end_time = get_time_nanoseconds();
                    core_orderbook_time = end_time - start_time;
                    
                    std::cout << Colors::RED << "✓ Order " << action.orderId_ << " cancelled successfully." << Colors::RESET << "\n";
                    break;
                }
                case ActionType::Modify:
                {
                    auto modify_request = CreateOrderModify(action);
                    // Time only the core OrderBook operation
                    uint64_t start_time = get_time_nanoseconds();
                    trades = orderbook_.modify_order(modify_request);
                    uint64_t end_time = get_time_nanoseconds();
                    core_orderbook_time = end_time - start_time;
                    
                    std::cout << Colors::YELLOW << "✓ Order " << action.orderId_ << " modified successfully." << Colors::RESET << "\n";
                    ShowTrades(trades);
                    break;
                }
                case ActionType::Show:
                {
                    ShowOrderBook();
                    break;
                }
                case ActionType::Help:
                {
                    ShowHelp();
                    break;
                }
                case ActionType::Quit:
                {
                    std::cout << Colors::CYAN << "Goodbye!" << Colors::RESET << "\n";
                    return;
                }
                case ActionType::Invalid:
                {
                    std::cout << Colors::RED << "Invalid command. Type 'help' for available commands." << Colors::RESET << "\n";
                    continue;
                }
                }
                
                // Show core OrderBook timing only for operations that modify the book
                if (core_orderbook_time > 0)
                {
                    std::cout << Colors::BLUE << "Core OrderBook time: " << Colors::BOLD << core_orderbook_time << " ns" << Colors::RESET;
                    if (core_orderbook_time >= 1000)
                        std::cout << Colors::BLUE << " (" << std::fixed << std::setprecision(2) << core_orderbook_time / 1000.0 << " μs)" << Colors::RESET;
                    std::cout << "\n";
                }
                std::cout << "\n";
            }
            catch (const std::exception& e)
            {
                std::cout << Colors::RED << "Error: " << e.what() << Colors::RESET << "\n\n";
            }
        }
    }
};

int main()
{
    try
    {
        OrderBookApp app;
        app.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
