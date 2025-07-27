#ifndef ENGINE_SERVER_HPP
#define ENGINE_SERVER_HPP

#include "OrderBook.hpp"
#include "Session.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <set>

class EngineServer {
public:
    EngineServer(boost::asio::io_context& io_context, short port);
    void shutdown();
    void broadcastToAllClients(const std::string& message);
    void broadcastToOthers(const std::string& message, Session* excludeSession);
    const std::set<std::shared_ptr<Session>>& getSessions() const;

private:
    void doAccept();
    void handleClientDisconnect(const std::string& clientAddress);

    boost::asio::ip::tcp::acceptor acceptor_;
    OrderBook orderBook_;
    std::set<std::shared_ptr<Session>> sessions_;
    bool shutdownRequested_;
};

#endif
