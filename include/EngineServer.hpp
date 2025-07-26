#ifndef ENGINE_SERVER_HPP
#define ENGINE_SERVER_HPP

#include "OrderBook.hpp"
#include <boost/asio.hpp>
#include <memory>

class EngineServer {
public:
    EngineServer(boost::asio::io_context& io_context, short port);

private:
    void doAccept();
    void handleClient(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket);

    boost::asio::ip::tcp::acceptor acceptor_;
    OrderBook orderBook_;
};

#endif
