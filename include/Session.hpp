#ifndef SESSION_HPP
#define SESSION_HPP

#include "OrderBook.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <functional>
#include <queue>

class EngineServer; // Forward declaration

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(boost::asio::ip::tcp::socket socket, OrderBook& orderBook,
            std::function<void(const std::string&)> disconnectCallback,
            EngineServer* server);
    ~Session();

    void start();
    std::string getClientAddress() const;
    void sendMessage(const std::string& message);

private:
    void doRead();
    void doWrite(const std::string& message);
    void handleDisconnect();
    void sendWelcomeMessage();

    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf buffer_;
    OrderBook& orderBook_;
    std::function<void(const std::string&)> disconnectCallback_;
    std::string clientAddress_;
    EngineServer* server_;
    std::queue<std::string> writeQueue_;
    bool writing_;
    bool registered_;
    bool disconnected_;  // Track if disconnect has already been handled
};

#endif
