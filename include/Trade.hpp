#ifndef TRADE_HPP
#define TRADE_HPP

#include <string>

class Trade {
public:
    int buyOrderId;
    int sellOrderId;
    double price;
    int quantity;

    Trade(int buyOrderId, int sellOrderId, double price, int quantity);

    std::string toString() const;
};

#endif
