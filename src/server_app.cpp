#include "server_rpc.hpp"
#include <cstdint>
#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    try {
        uint16_t port = static_cast<uint16_t>(std::stoi(argv[1]));
        rpc::Server server(port);

        server.run();

    } catch (const std::exception& e) {
        std::cerr << "[SERVER ERROR]: " << e.what() << '\n';
        return 1;
    }

    return 0;
}