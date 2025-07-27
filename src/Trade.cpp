#include "Trade.hpp"
#include <sstream>
#include <iomanip>

Trade::Trade(int buyOrderId, int sellOrderId, double price, int quantity)
    : buyOrderId(buyOrderId), sellOrderId(sellOrderId), price(price), quantity(quantity) {}

std::string Trade::toString() const {
    std::ostringstream oss;
    oss << "TRADE BuyID: " << buyOrderId
        << ", SellID: " << sellOrderId
        << ", Price: " << price
        << ", Quantity: " << quantity;
    return oss.str();
}
