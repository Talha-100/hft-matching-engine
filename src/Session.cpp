#include "Session.hpp"
#include "EngineServer.hpp"
#include "MarketPublisher.hpp"
#include <iostream>
#include <sstream>
#include <chrono>

using boost::asio::ip::tcp;

Session::Session(tcp::socket socket, OrderBook& orderBook,
                 std::function<void(const std::string&)> disconnectCallback,
                 EngineServer* server)
    : socket_(std::move(socket)), orderBook_(orderBook),
      disconnectCallback_(disconnectCallback), server_(server), writing_(false), registered_(false), disconnected_(false) {
    clientAddress_ = socket_.remote_endpoint().address().to_string();
}

Session::~Session() {
    try {
        // Ensure socket is closed
        if (socket_.is_open()) {
            boost::system::error_code ec;
            socket_.close(ec);
            // Ignore errors during cleanup
        }

        // Note: MarketPublisher cleanup happens automatically via weak_ptr expiration
        // We can't call shared_from_this() in destructor, but that's okay because
        // MarketPublisher uses weak_ptr and will clean up expired sessions automatically
    } catch (const std::exception& e) {
        std::cerr << "Error during session cleanup: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown error during session cleanup" << std::endl;
    }
}

void Session::start() {
    // Register with MarketPublisher first
    std::weak_ptr<Session> weakSelf = shared_from_this();
    MarketPublisher::getInstance().registerSession(weakSelf);
    registered_ = true;

    // Get total client count and display connection message
    size_t totalClients = MarketPublisher::getInstance().getSessionCount();
    std::cout << "Client connected: " << clientAddress_
              << " (Total active clients: " << totalClients << ")" << std::endl;

    sendWelcomeMessage();
    doRead();
}

std::string Session::getClientAddress() const {
    return clientAddress_;
}

void Session::sendMessage(const std::string& message) {
    auto self(shared_from_this());
    boost::asio::post(socket_.get_executor(),
        [this, message]() {
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
    welcome << "------------------------------------\n";
    welcome << "Available Commands:\n";
    welcome << "  BUY <price> <quantity>   - Place a buy order\n";
    welcome << "  SELL <price> <quantity>  - Place a sell order\n";
    welcome << "  CANCEL <orderId>         - Cancel an existing order\n";
    welcome << "  DC                       - Disconnect from server\n";
    welcome << "\nExample: BUY 100.50 25\n";
    welcome << "         SELL 101.00 10\n";
    welcome << "         CANCEL 5\n";
    welcome << "====================================\n\n";

    sendMessage(welcome.str());
}

void Session::doRead() {
    // Check if disconnected before starting new read operation
    if (disconnected_ || !socket_.is_open()) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::async_read_until(socket_, buffer_, '\n',
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec && !disconnected_) {
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

                if (command == "DC") {
                    response << "Disconnecting...\n\n";
                    sendMessage(response.str());
                    // Give time for message to be sent before closing
                    auto timer = std::make_shared<boost::asio::steady_timer>(socket_.get_executor());
                    timer->expires_after(std::chrono::milliseconds(100));
                    timer->async_wait([this, self, timer](boost::system::error_code) {
                        // Actually close the socket to disconnect
                        socket_.close();
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
                    std::string priceStr, quantityStr;

                    if (!(iss >> priceStr >> quantityStr)) {
                        response << "INVALID INPUT\n\n";
                    } else {
                        try {
                            // Parse price - both integers and decimals are valid
                            double price = std::stod(priceStr);
                            int quantity = std::stoi(quantityStr);

                            // Validation: both must be positive
                            if (price <= 0.0 || quantity <= 0) {
                                response << "INVALID INPUT\n\n";
                            } else {
                                OrderType type = (command == "BUY") ? OrderType::BUY : OrderType::SELL;

                                // Debug logging
                                std::cout << "Processing order: [" << command << " " << price << " " << quantity
                                         << "] from " << clientAddress_ << std::endl;

                                int orderId = orderBook_.addOrder(type, price, quantity);
                                orderBook_.matchOrders();

                                // Get new trades after matching
                                auto newTrades = orderBook_.getRecentTrades();

                                response << "CONFIRMED OrderID: " << orderId << "\n\n";

                                std::cout << "Generated " << newTrades.size() << " trades" << "\n" << std::endl;

                                // Send detailed trade info to this client and broadcast market data to others
                                for (const auto& trade : newTrades) {
                                    try {
                                        std::string tradeMsg = trade.toString() + "\n\n";
                                        response << tradeMsg;

                                        // Broadcast clean market data to other clients via MarketPublisher
                                        std::weak_ptr weakSelf = shared_from_this();
                                        MarketPublisher::getInstance().broadcastTradeToMarket(trade, weakSelf);
                                    } catch (const std::exception& e) {
                                        std::cerr << "Error processing trade: " << e.what() << std::endl;
                                    }
                                }
                            }
                        } catch (const std::invalid_argument& e) {
                            std::cerr << "Invalid number format in order from " << clientAddress_
                                     << ": " << priceStr << " " << quantityStr << std::endl;
                            response << "INVALID INPUT\n\n";
                        } catch (const std::out_of_range& e) {
                            std::cerr << "Number out of range in order from " << clientAddress_
                                     << ": " << priceStr << " " << quantityStr << std::endl;
                            response << "INVALID INPUT\n\n";
                        } catch (const std::exception& e) {
                            std::cerr << "Error parsing order from " << clientAddress_ << ": " << e.what() << std::endl;
                            response << "INVALID INPUT\n\n";
                        }
                    }
                } else {
                    response << "INVALID INPUT\n\n";
                }

                sendMessage(response.str());
                doRead(); // Continue reading
            } else {
                handleDisconnect();
            }
        });
}

void Session::doWrite(const std::string& message) {
    // Check if disconnected before attempting write
    if (disconnected_ || !socket_.is_open()) {
        writing_ = false;
        return;
    }

    auto self(shared_from_this());

    if (writeQueue_.empty()) {
        writing_ = false;
        return; // Don't call doRead from here - let the caller handle it
    }

    writing_ = true;
    std::string msg = writeQueue_.front();
    writeQueue_.pop();

    boost::asio::async_write(socket_, boost::asio::buffer(msg),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec && !disconnected_) {
                doWrite(""); // Process next message in queue
            } else {
                writing_ = false;
                handleDisconnect();
            }
        });
}

void Session::handleDisconnect() {
    // Prevent multiple disconnect handling
    if (disconnected_) {
        return;
    }
    disconnected_ = true;

    std::cout << "Client disconnected: " << clientAddress_ << std::endl;
    if (disconnectCallback_) {
        disconnectCallback_(clientAddress_);
    }
}
