
#pragma once

enum class OrderType{
    GoodTillCancel,
    FillAndKill,
    Market,
    FillOrKill,
    GoodForDay
    
};

/*
GoodTillCancel- Added to the Order book at the given price, may not execute trades immediately but will persist until cancelled or executed completely.
FillAndKill- When Adding this the OB is already saturated, now we need to check if at all this can be matched, if so we add it and remove it in the end if partially executed
FillOrKill- ?
Market - ?
Limit - Same as GoodTillCancel.
*/