#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include "Order.hpp"
#include <vector>
#include <deque>
#include "Trade.hpp"

class OrderBook {
private:
    std::deque<Order> buyOrders;
    std::deque<Order> sellOrders;
    int nextOrderId = 1;
    void sortOrders();
    std::vector<Trade> trades;

public:
    int addOrder(OrderType type, double price, int quantity);
    bool cancelOrder(int orderId);
    void printOrderBook() const;
    void matchOrders();
    const std::vector<Trade>& getTradeHistory() const;
};

#endif
