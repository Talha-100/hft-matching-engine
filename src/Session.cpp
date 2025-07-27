#include "Session.hpp"
#include "EngineServer.hpp"
#include <iostream>
#include <sstream>
#include <chrono>

using boost::asio::ip::tcp;

Session::Session(tcp::socket socket, OrderBook& orderBook,
                 std::function<void(const std::string&)> disconnectCallback,
                 EngineServer* server)
    : socket_(std::move(socket)), orderBook_(orderBook),
      disconnectCallback_(disconnectCallback), server_(server), writing_(false) {
    clientAddress_ = socket_.remote_endpoint().address().to_string();
}

void Session::start() {
    std::cout << "Client connected: " << clientAddress_ << std::endl;
    sendWelcomeMessage();
    doRead();
}

std::string Session::getClientAddress() const {
    return clientAddress_;
}

void Session::sendMessage(const std::string& message) {
    auto self(shared_from_this());
    boost::asio::post(socket_.get_executor(),
        [this, self, message]() {
            writeQueue_.push(message);
            if (!writing_) {
                doWrite("");
            }
        });
}

void Session::sendWelcomeMessage() {
    std::ostringstream welcome;
    welcome << "====================================\n";
    welcome << "  HFT Matching Engine - Welcome!\n";
    welcome << "====================================\n";
    welcome << "Available Commands:\n";
    welcome << "  BUY <price> <quantity>   - Place a buy order\n";
    welcome << "  SELL <price> <quantity>  - Place a sell order\n";
    welcome << "  CANCEL <orderId>         - Cancel an existing order\n";
    welcome << "  DISCONNECT               - Disconnect from server\n";
    welcome << "\nExample: BUY 100.50 25\n";
    welcome << "         SELL 101.00 10\n";
    welcome << "         CANCEL 5\n";
    welcome << "====================================\n\n";

    sendMessage(welcome.str());
}

void Session::doRead() {
    auto self(shared_from_this());
    boost::asio::async_read_until(socket_, buffer_, '\n',
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                std::istream is(&buffer_);
                std::string line;
                std::getline(is, line);

                std::istringstream iss(line);
                std::string command;

                if (!(iss >> command)) {
                    sendMessage("INVALID INPUT\n\n");
                    return;
                }

                std::ostringstream response;

                if (command == "DISCONNECT") {
                    response << "Disconnecting...\n\n";
                    sendMessage(response.str());
                    // Give time for message to be sent before closing
                    auto timer = std::make_shared<boost::asio::steady_timer>(socket_.get_executor());
                    timer->expires_after(std::chrono::milliseconds(100));
                    timer->async_wait([this, self, timer](boost::system::error_code) {
                        handleDisconnect();
                    });
                    return;
                } else if (command == "CANCEL") {
                    int orderId;
                    if (!(iss >> orderId) || orderId <= 0) {
                        response << "INVALID INPUT\n\n";
                    } else {
                        bool cancelled = orderBook_.cancelOrder(orderId);
                        if (cancelled) {
                            std::cout << "Order cancelled: OrderID=" << orderId << std::endl;
                            response << "CANCELLED OrderID: " << orderId << "\n\n";
                        } else {
                            response << "ORDER NOT FOUND: " << orderId << "\n\n";
                        }
                    }
                } else if (command == "BUY" || command == "SELL") {
                    double price;
                    int quantity;

                    if (!(iss >> price >> quantity) || price <= 0 || quantity <= 0) {
                        response << "INVALID INPUT\n\n";
                    } else {
                        OrderType type = (command == "BUY") ? OrderType::BUY : OrderType::SELL;
                        int orderId = orderBook_.addOrder(type, price, quantity);
                        orderBook_.matchOrders();

                        // Get new trades after matching
                        auto newTrades = orderBook_.getRecentTrades();

                        response << "CONFIRMED OrderID=" << orderId << "\n\n";

                        // Send trades to this client and broadcast to others
                        for (const auto& trade : newTrades) {
                            std::string tradeMsg = trade.toString() + "\n\n";
                            response << tradeMsg;

                            // Broadcast to other clients with MARKET prefix
                            if (server_) {
                                std::string marketMsg = "MARKET " + tradeMsg;
                                server_->broadcastToOthers(marketMsg, this);
                            }
                        }
                    }
                } else {
                    response << "INVALID INPUT\n\n";
                }

                sendMessage(response.str());
            } else {
                handleDisconnect();
            }
        });
}

void Session::doWrite(const std::string& message) {
    auto self(shared_from_this());

    if (writeQueue_.empty()) {
        writing_ = false;
        doRead();
        return;
    }

    writing_ = true;
    std::string msg = writeQueue_.front();
    writeQueue_.pop();

    boost::asio::async_write(socket_, boost::asio::buffer(msg),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                doWrite("");  // Process next message in queue
            } else {
                handleDisconnect();
            }
        });
}

void Session::handleDisconnect() {
    std::cout << "Client disconnected: " << clientAddress_ << std::endl;
    if (disconnectCallback_) {
        disconnectCallback_(clientAddress_);
    }
}
