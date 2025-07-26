#include <gtest/gtest.h>
#include "OrderBook.hpp"
#include "Order.hpp"
#include <sstream>
#include <iostream>

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook orderBook;
};

// Test Order class
TEST(OrderTest, OrderCreation) {
    Order order(1, OrderType::BUY, 100.0, 10);
    EXPECT_EQ(order.id, 1);
    EXPECT_EQ(order.type, OrderType::BUY);
    EXPECT_DOUBLE_EQ(order.price, 100.0);
    EXPECT_EQ(order.quantity, 10);
}

TEST(OrderTest, OrderToString) {
    Order buyOrder(1, OrderType::BUY, 100.5, 10);
    Order sellOrder(2, OrderType::SELL, 99.0, 5);

    std::string buyStr = buyOrder.to_string();
    std::string sellStr = sellOrder.to_string();

    EXPECT_TRUE(buyStr.find("BUY") != std::string::npos);
    EXPECT_TRUE(buyStr.find("100.5") != std::string::npos);
    EXPECT_TRUE(sellStr.find("SELL") != std::string::npos);
    EXPECT_TRUE(sellStr.find("99") != std::string::npos);
}

// Test OrderBook basic functionality
TEST_F(OrderBookTest, AddSingleOrder) {
    int orderId = orderBook.addOrder(OrderType::BUY, 100.0, 10);
    EXPECT_GT(orderId, 0);
}

TEST_F(OrderBookTest, AddMultipleOrders) {
    int buyId = orderBook.addOrder(OrderType::BUY, 100.0, 10);
    int sellId = orderBook.addOrder(OrderType::SELL, 101.0, 5);

    EXPECT_GT(buyId, 0);
    EXPECT_GT(sellId, 0);
    EXPECT_NE(buyId, sellId);
}

TEST_F(OrderBookTest, CancelExistingOrder) {
    int orderId = orderBook.addOrder(OrderType::BUY, 100.0, 10);
    bool cancelled = orderBook.cancelOrder(orderId);
    EXPECT_TRUE(cancelled);
}

TEST_F(OrderBookTest, CancelNonExistentOrder) {
    bool cancelled = orderBook.cancelOrder(999);
    EXPECT_FALSE(cancelled);
}

// Test matching logic
TEST_F(OrderBookTest, NoMatchWhenBuyPriceTooLow) {
    std::ostringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    orderBook.addOrder(OrderType::BUY, 99.0, 10);
    orderBook.addOrder(OrderType::SELL, 100.0, 5);
    orderBook.matchOrders();

    std::cout.rdbuf(old);
    std::string output = buffer.str();
    EXPECT_TRUE(output.find("TRADE EXECUTED") == std::string::npos);
}

TEST_F(OrderBookTest, MatchWhenBuyPriceEqualsSellPrice) {
    std::ostringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    orderBook.addOrder(OrderType::BUY, 100.0, 10);
    orderBook.addOrder(OrderType::SELL, 100.0, 5);
    orderBook.matchOrders();

    std::cout.rdbuf(old);
    std::string output = buffer.str();
    EXPECT_TRUE(output.find("TRADE") != std::string::npos);
    EXPECT_TRUE(output.find("Quantity=5") != std::string::npos);
}

TEST_F(OrderBookTest, MatchWhenBuyPriceHigherThanSellPrice) {
    std::ostringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    orderBook.addOrder(OrderType::BUY, 101.0, 10);
    orderBook.addOrder(OrderType::SELL, 100.0, 5);
    orderBook.matchOrders();

    std::cout.rdbuf(old);
    std::string output = buffer.str();
    EXPECT_TRUE(output.find("TRADE") != std::string::npos);
    EXPECT_TRUE(output.find("Quantity=5") != std::string::npos);
    EXPECT_TRUE(output.find("Price=100") != std::string::npos);
}

TEST_F(OrderBookTest, PartialFillBuyOrder) {
    std::ostringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    orderBook.addOrder(OrderType::BUY, 100.0, 10);
    orderBook.addOrder(OrderType::SELL, 100.0, 5);
    orderBook.matchOrders();

    std::cout.rdbuf(old);
    std::string output = buffer.str();
    EXPECT_TRUE(output.find("Quantity=5") != std::string::npos);

    buffer.str("");
    buffer.clear();
    std::cout.rdbuf(buffer.rdbuf());

    orderBook.printOrderBook();
    std::cout.rdbuf(old);
    output = buffer.str();
    EXPECT_TRUE(output.find("Quantity=5") != std::string::npos);
}

TEST_F(OrderBookTest, PricePriorityMatching) {
    std::ostringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    int lowBuyId = orderBook.addOrder(OrderType::BUY, 99.0, 5);
    int highBuyId = orderBook.addOrder(OrderType::BUY, 101.0, 5);
    orderBook.addOrder(OrderType::SELL, 100.0, 5);

    orderBook.matchOrders();

    std::cout.rdbuf(old);
    std::string output = buffer.str();

    EXPECT_TRUE(output.find("BuyID=" + std::to_string(highBuyId)) != std::string::npos);
    EXPECT_TRUE(output.find("BuyID=" + std::to_string(lowBuyId)) == std::string::npos);
}

TEST_F(OrderBookTest, TimePriorityMatching) {
    std::ostringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    int firstBuyId = orderBook.addOrder(OrderType::BUY, 100.0, 5);
    int secondBuyId = orderBook.addOrder(OrderType::BUY, 100.0, 5);
    orderBook.addOrder(OrderType::SELL, 100.0, 5);

    orderBook.matchOrders();

    std::cout.rdbuf(old);
    std::string output = buffer.str();

    EXPECT_TRUE(output.find("BuyID=" + std::to_string(firstBuyId)) != std::string::npos);
    EXPECT_TRUE(output.find("BuyID=" + std::to_string(secondBuyId)) == std::string::npos);
}

TEST_F(OrderBookTest, MultipleMatches) {
    std::ostringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    orderBook.addOrder(OrderType::BUY, 101.0, 5);
    orderBook.addOrder(OrderType::BUY, 100.0, 5);
    orderBook.addOrder(OrderType::SELL, 99.0, 8);

    orderBook.matchOrders();
    auto trades = orderBook.getTradeHistory();

    std::cout.rdbuf(old);
    std::string output = buffer.str();

    size_t firstTrade = output.find("TRADE");
    EXPECT_NE(firstTrade, std::string::npos);
    size_t secondTrade = output.find("TRADE", firstTrade + 1);
    EXPECT_NE(secondTrade, std::string::npos);

    ASSERT_EQ(trades.size(), 2);
}