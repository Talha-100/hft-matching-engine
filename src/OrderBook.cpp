#include "OrderBook.hpp"
#include "Trade.hpp"
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

void OrderBook::sortOrders() {
    // Sort orders by price first and then by ID (time priority)

    std::sort(buyOrders.begin(), buyOrders.end(),
        [](const Order& a, const Order& b) {
            if (a.price != b.price) { return a.price > b.price; }
            return a.id < b.id;
        });

    std::sort(sellOrders.begin(), sellOrders.end(),
        [](const Order& a, const Order& b) {
            if (a.price != b.price) { return a.price < b.price; }
            return a.id < b.id;
        });
}

void OrderBook::matchOrders() {
    sortOrders();

    while (!buyOrders.empty() && !sellOrders.empty()) {
        Order& bestBuy = buyOrders.front();
        Order& bestSell = sellOrders.front();

        if (bestBuy.price < bestSell.price) {
            break; // No more matches possible
        }

        // Execute the trade at the sell price
        int tradeQuantity = std::min(bestBuy.quantity, bestSell.quantity);
        double tradePrice = bestSell.price;

        trades.emplace_back(bestBuy.id, bestSell.id, tradePrice, tradeQuantity);

        bestBuy.quantity -= tradeQuantity;
        bestSell.quantity -= tradeQuantity;

        // Remove fully filled orders
        if (bestBuy.quantity == 0) {
            buyOrders.pop_front();
        }
        if (bestSell.quantity == 0) {
            sellOrders.pop_front();
        }
    }
}

const std::vector<Trade>& OrderBook::getTradeHistory() const {
    return trades;
}
