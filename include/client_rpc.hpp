#pragma once

#include "protocol.hpp"
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>

namespace rpc {

class File {
   public:
    explicit File(int fd) : server_fd_{fd} {}
    int fd() const { return server_fd_; }

   private:
    int server_fd_;
};

class Client {
   public:
    Client(std::string_view server_ip, uint16_t port);
    ~Client();

    std::optional<File> open(std::string_view pathname, std::string_view mode);
    std::ptrdiff_t read(const File& file, std::span<std::byte> buffer);
    std::optional<int64_t> seek(const File& file, int64_t offset, rpc::protocol::SeekWhence whence);
    std::ptrdiff_t write(const File& file, std::span<const std::byte> buffer);

    bool chmod(std::string_view pathname, uint32_t mode);
    bool unlink(std::string_view pathname);
    bool rename(std::string_view oldpath, std::string_view newpath);

   private:
    uint64_t auth_token_{0};
    uint64_t seq_num_{0};
    int sock_fd_{-1};

    std::optional<protocol::Response> send_with_retry(protocol::Request& req);
};

}  // namespace rpc