#include <iostream>
#include "OrderBook.hpp"

int main() {
    std::cout << "Welcome to hft-sim 🚀\n" << std::endl;

    OrderBook book;
    book.addOrder(OrderType::BUY, 100.5, 10);
    book.addOrder(OrderType::SELL, 101.0, 15);
    book.addOrder(OrderType::BUY, 99.0, 5);

    book.printOrderBook();

    book.cancelOrder(2);  // Try cancelling sell order

    std::cout << "\nAfter cancelling order 2:\n";
    book.printOrderBook();

    return 0;
}
