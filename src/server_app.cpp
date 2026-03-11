#include "server_rpc.hpp"

int main() {
    RpcServer server(8080);
    server.run();
    return 0;
}