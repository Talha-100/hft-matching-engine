#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include "Order.hpp"
#include <vector>
#include <deque>

class OrderBook {
private:
    std::deque<Order> buyOrders;
    std::deque<Order> sellOrders;
    int nextOrderId = 1;

public:
    int addOrder(OrderType type, double price, int quantity);
    bool cancelOrder(int orderId);
    void printOrderBook() const;
};

#endif
