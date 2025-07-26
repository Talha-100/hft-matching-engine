#include "EngineServer.hpp"
#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        boost::asio::io_context io;
        EngineServer server(io, 12345); // Choose any port
        io.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
