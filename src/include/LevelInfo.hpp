#pragma once
#include "Usings.hpp"
#include <functional>
#include <iostream>
#include <map>

struct LevelInfo{//single level's worth
    Price price_;
    Quantity quantity_;
    int count_;
    
    void display() const{//format later.
        std::cout<<"Price: "<<price_<<" "<<"Quantity: "<<quantity_<<" "<<"Number of Orders: "<<count_<<std::endl;
    }
    
    };
    
struct LevelsInfo{//contains two vectors one each for buy and sell
    std::map<Price,LevelInfo, std::greater<Price>> buy_levels_;
    std::map<Price,LevelInfo,std::less<Price>> sell_levels_;
    void display_buy_levels() const{
        std::cout<<"[Buy Orders in the system]"<<std::endl;
        std::cout<<"#"<<buy_levels_.size()<<std::endl;
        for (auto [_,level]: buy_levels_){
            level.display();
        }
    } 
    void display_sell_levels() const{
        std::cout<<"[Sell Orders in the system]"<<std::endl;
        std::cout<<"#"<<sell_levels_.size()<<std::endl;

        for (auto it = sell_levels_.rbegin(); it != sell_levels_.rend(); ++it){
            it->second.display(); 
        }
    }
    void display_all_levels() const{
        std::cout<<"-------------------------------------------------------------------------------------------------------"<<std::endl;
        display_sell_levels();
        display_buy_levels();
        std::cout<<"-------------------------------------------------------------------------------------------------------"<<std::endl;

    }
    
    auto& get_bids() const{
        return buy_levels_;
    }

    auto & get_asks() const{
        return sell_levels_;
    }
};