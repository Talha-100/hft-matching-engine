#include "EngineServer.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <algorithm>

using boost::asio::ip::tcp;

EngineServer::EngineServer(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), shutdownRequested_(false) {
    doAccept();
}

void EngineServer::doAccept() {
    if (shutdownRequested_) return;

    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec && !shutdownRequested_) {
                auto session = std::make_shared<Session>(
                    std::move(socket),
                    orderBook_,
                    [this](const std::string& clientAddress) {
                        handleClientDisconnect(clientAddress);
                    },
                    this
                );
                sessions_.insert(session);
                session->start();
            } else if (ec && !shutdownRequested_) {
                std::cerr << "Accept error: " << ec.message() << std::endl;
            }
            if (!shutdownRequested_) {
                doAccept();
            }
        });
}

void EngineServer::handleClientDisconnect(const std::string& clientAddress) {
    // Remove disconnected session from active sessions
    auto it = std::find_if(sessions_.begin(), sessions_.end(),
        [&clientAddress](const std::shared_ptr<Session>& session) {
            return session->getClientAddress() == clientAddress;
        });

    if (it != sessions_.end()) {
        sessions_.erase(it);
    }

    // Log the disconnection and current client count
    std::cout << "Total active clients: " << sessions_.size() << std::endl;
}

void EngineServer::shutdown() {
    shutdownRequested_ = true;

    // Notify all clients about server shutdown
    broadcastToAllClients("SERVER SHUTDOWN: Server is shutting down. Disconnecting...\n\n");

    // Close all sessions
    for (auto& session : sessions_) {
        // Sessions will handle their own cleanup
    }
    sessions_.clear();

    // Close acceptor
    acceptor_.close();

    std::cout << "All clients disconnected. Server shutdown complete." << std::endl;
}

void EngineServer::broadcastToAllClients(const std::string& message) {
    for (auto& session : sessions_) {
        session->sendMessage(message);
    }
}

void EngineServer::broadcastToOthers(const std::string& message, Session* excludeSession) {
    for (auto& session : sessions_) {
        if (session.get() != excludeSession) {
            session->sendMessage(message);
        }
    }
}

const std::set<std::shared_ptr<Session>>& EngineServer::getSessions() const {
    return sessions_;
}