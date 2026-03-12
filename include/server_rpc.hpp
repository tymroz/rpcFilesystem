#pragma once

#include <cstdint>

namespace rpc {

class Server {
   public:
    explicit Server(uint16_t port);
    ~Server();

    void run();

   private:
    int sock_fd_{-1};
};

}  // namespace rpc