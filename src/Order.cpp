#include "Order.hpp"
#include <sstream>

Order::Order(int id, OrderType type, double price, int quantity)
    : id(id), type(type), price(price), quantity(quantity) {}

std::string Order::to_string() const {
    std::ostringstream oss;
    oss << "Order[ID=" << id
        << ", Type=" << (type == OrderType::BUY ? "BUY" : "SELL")
        << ", Price=" << price
        << ", Quantity=" << quantity << "]";
    return oss.str();
}
