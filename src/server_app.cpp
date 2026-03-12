#include "server_rpc.hpp"
#include <exception>
#include <iostream>

int main() {
    try {
        constexpr uint16_t PORT = 8080;
        rpc::Server server(PORT);

        server.run();

    } catch (const std::exception &e) {
        std::cerr << "[SERVER ERROR]: " << e.what() << '\n';
        return 1;
    }

    return 0;
}