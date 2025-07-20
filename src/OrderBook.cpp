#include "OrderBook.hpp"
#include <iostream>
#include <algorithm>

int OrderBook::addOrder(const OrderType type, double price, int quantity) {
    const Order order(nextOrderId++, type, price, quantity);
    if (type == OrderType::BUY) {
        buyOrders.push_back(order);
    } else {
        sellOrders.push_back(order);
    }
    return order.id;
}

bool OrderBook::cancelOrder(int orderId) {
    auto cancel = [orderId](std::deque<Order>& orders) {
        auto it = std::remove_if(orders.begin(), orders.end(),
            [orderId](const Order& o) { return o.id == orderId; });
        if (it != orders.end()) {
            orders.erase(it, orders.end());
            return true;
        }
        return false;
    };
    return cancel(buyOrders) || cancel(sellOrders);
}
void OrderBook::printOrderBook() const {
    std::cout << "Buy Orders:\n";
    for (const auto& order : buyOrders) {
        std::cout << order.to_string() << "\n";
    }
    std::cout << "Sell Orders:\n";
    for (const auto& order : sellOrders) {
        std::cout << order.to_string() << "\n";
    }
}
