#ifndef ORDER_HPP
#define ORDER_HPP

#include <string>

enum class OrderType{
    BUY,
    SELL
};

class Order {
public:
    int id;
    OrderType type;
    double price;
    int quantity;

    Order(int id, OrderType type, double price, int quantity);

    std::string to_string() const;
};

#endif
