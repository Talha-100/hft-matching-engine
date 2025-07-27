#include "EngineServer.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <csignal>

int main() {
    try {
        const short port = 12345;
        boost::asio::io_context io;
        EngineServer server(io, port);

        std::cout << "=== HFT Matching Engine Server ===" << std::endl;
        std::cout << "Server started on port " << port << std::endl;
        std::cout << "Press Ctrl+C or type 'shutdown' to gracefully stop the server" << std::endl;
        std::cout << "====================================" << std::endl;

        // Set up signal handler for Ctrl+C
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&server, &io](boost::system::error_code, int) {
            std::cout << "\nReceived shutdown signal. Shutting down server..." << std::endl;
            server.shutdown();
            io.stop();
        });

        // Alternative: Handle stdin input asynchronously
        boost::asio::posix::stream_descriptor input(io, STDIN_FILENO);
        std::string command;

        std::function<void()> readCommand = [&]() {
            boost::asio::async_read_until(input, boost::asio::dynamic_buffer(command), '\n',
                [&](boost::system::error_code ec, std::size_t) {
                    if (!ec) {
                        if (command.find("shutdown") != std::string::npos) {
                            std::cout << "Shutting down server..." << std::endl;
                            server.shutdown();
                            io.stop();
                        } else if (!command.empty() && command != "\n") {
                            std::cout << "Unknown command. Type 'shutdown' or press Ctrl+C to stop." << std::endl;
                        }
                        command.clear();
                        readCommand(); // Continue reading
                    }
                });
        };

        readCommand(); // Start reading commands

        // Run the event loop (single-threaded)
        io.run();

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
