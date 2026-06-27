#pragma once
#include <iostream>
#include "Usings.hpp"
class TradeInfo{
    public:
    struct SideInfoTrade{
        OrderId id_;
        Price price_;
    };

    TradeInfo(SideInfoTrade buy, SideInfoTrade sell, Price trade_price, Quantity quantity):
    buy_(buy), sell_(sell), trade_price_(trade_price), quantity_(quantity) {}

    void print(){
        std::cout<<"Trade: Buy Order #"<<buy_.id_<<" matched with sell order #"<<sell_.id_<<" at  Price="<<trade_price_<<" and Quanity="<<quantity_<<std::endl;
    }

    private:
    SideInfoTrade buy_,sell_;
    Quantity quantity_; //the quantity which got traded.
    Price trade_price_; //this is the price at which the actual trade happened
};


class TradeInfos{
    public:
    
    void print_stats(){std::cout<<trades_made_.size()<<" numbers of orders executed"<<std::endl;}
    void print_all_trades(){
        for (auto trade: trades_made_){
            trade.print();
        }
    }
    template<typename ...Ts>
    void emplace_back(Ts&&... args){
        trades_made_.emplace_back(std::forward<Ts>(args)...);
    }
    
        std::vector<TradeInfo> trades_made_;
    };