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
                    handleClient(socket);
                } else {
                    std::cerr << "Accept error: " << error.message() << std::endl;
                }
                doAccept();
            }));
}

void EngineServer::handleClient(const std::shared_ptr<tcp::socket>& socket) {
    std::cout << "Client connected: " << socket->remote_endpoint().address().to_string() << std::endl;

    auto buffer = std::make_shared<boost::asio::streambuf>();

    boost::asio::async_read_until(*socket, *buffer, '\n',
        boost::asio::bind_executor(socket->get_executor(),
            [this, socket, buffer](const boost::system::error_code& error, std::size_t length) {
                if (!error) {
                    std::istream is(buffer.get());
                    std::string line;
                    std::getline(is, line);

                    std::istringstream iss(line);
                    std::string typeStr;
                    double price;
                    int quantity;

                    if (iss >> typeStr >> price >> quantity) {
                        OrderType type = (typeStr == "BUY") ? OrderType::BUY : OrderType::SELL;
                        int orderId = orderBook_.addOrder(type, price, quantity);
                        orderBook_.matchOrders();

                        std::ostringstream response;
                        response << "CONFIRMED OrderID=" << orderId << "\n";
                        for (const auto& trade : orderBook_.getTradeHistory()) {
                            response << trade.toString() << "\n";
                        }

                        boost::asio::async_write(*socket, boost::asio::buffer(response.str()),
                            boost::asio::bind_executor(socket->get_executor(),
                                [this, socket](boost::system::error_code, std::size_t) {
                                    handleClient(socket);
                                }));
                    } else {
                        std::string err = "INVALID INPUT\n";
                        boost::asio::async_write(*socket, boost::asio::buffer(err),
                            boost::asio::bind_executor(socket->get_executor(),
                                [this, socket](boost::system::error_code, std::size_t) {
                                    handleClient(socket);
                                }));
                    }
                }
            }));
}