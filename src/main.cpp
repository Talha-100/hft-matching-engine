#include <iostream>
#include "OrderBook.hpp"

int main() {
    std::cout << "Welcome to hft-sim 🚀\n" << std::endl;

    OrderBook book;
    book.addOrder(OrderType::BUY, 100.5, 10);
    book.addOrder(OrderType::SELL, 101.0, 15);
    book.addOrder(OrderType::BUY, 99.0, 5);
    book.addOrder(OrderType::SELL, 100.0, 8);  // This should match with first buy order

    std::cout << "Before matching:\n";
    book.printOrderBook();

    std::cout << "\nMatching orders...\n";
    book.matchOrders();

    std::cout << "\nAfter matching:\n";
    book.printOrderBook();

    std::cout << std::endl;
    for (const auto& trade : book.getTrades()) {
        std::cout << trade.toString() << "\n";
    }

    return 0;
}
