#include "EngineServer.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <sstream>

using boost::asio::ip::tcp;

EngineServer::EngineServer(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
    doAccept();
}

void EngineServer::doAccept() {
    auto socket = std::make_shared<tcp::socket>(acceptor_.get_executor());
    acceptor_.async_accept(*socket,
        boost::asio::bind_executor(acceptor_.get_executor(),
            [this, socket](const boost::system::error_code& error) {
                if (!error) {
                    handleClient(socket, true); // true = new connection
                } else {
                    std::cerr << "Accept error: " << error.message() << std::endl;
                }
                doAccept();
            }));
}

void EngineServer::handleClient(const std::shared_ptr<tcp::socket>& socket, bool isNewConnection) {
    if (isNewConnection) {
        std::cout << "Client connected: " << socket->remote_endpoint().address().to_string() << std::endl;
    }

    handleClientRequest(socket);
}

void EngineServer::handleClientRequest(const std::shared_ptr<tcp::socket>& socket) {
    auto buffer = std::make_shared<boost::asio::streambuf>();

    boost::asio::async_read_until(*socket, *buffer, '\n',
        boost::asio::bind_executor(socket->get_executor(),
            [this, socket, buffer](const boost::system::error_code& error, std::size_t length) {
                if (!error) {
                    std::istream is(buffer.get());
                    std::string line;
                    std::getline(is, line);

                    std::istringstream iss(line);
                    std::string command;

                    if (!(iss >> command)) {
                        std::string err = "INVALID INPUT\n\n";
                        boost::asio::async_write(*socket, boost::asio::buffer(err),
                            boost::asio::bind_executor(socket->get_executor(),
                                [this, socket](boost::system::error_code, std::size_t) {
                                    handleClient(socket, false); // false = not a new connection
                                }));
                        return;
                    }

                    std::ostringstream response;

                    if (command == "CANCEL") {
                        int orderId;
                        if (!(iss >> orderId) || orderId <= 0) {
                            response << "INVALID INPUT\n\n";
                        } else {
                            bool cancelled = orderBook_.cancelOrder(orderId);
                            if (cancelled) {
                                std::cout << "Order cancelled: " << orderId << std::endl;
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

                            response << "CONFIRMED OrderID=" << orderId << "\n\n";
                            for (const auto& trade : orderBook_.getRecentTrades()) {
                                response << trade.toString() << "\n\n";
                            }
                        }
                    } else {
                        response << "INVALID INPUT\n\n";
                    }

                    boost::asio::async_write(*socket, boost::asio::buffer(response.str()),
                        boost::asio::bind_executor(socket->get_executor(),
                            [this, socket](boost::system::error_code, std::size_t) {
                                handleClient(socket, false); // false = not a new connection
                            }));
                }
            }));
}