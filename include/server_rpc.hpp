#pragma once

#include <cstdint>
#include <map>

namespace rpc {

struct FileState {
    uint64_t last_seq_num = 0;
    int32_t last_status = 0;
    int64_t last_offset_result = 0;
};

class Server {
   public:
    explicit Server(uint16_t port);
    ~Server();

    void run();

   private:
    int sock_fd_{-1};
    std::map<int32_t, FileState> fd_cache;
};

}  // namespace rpc