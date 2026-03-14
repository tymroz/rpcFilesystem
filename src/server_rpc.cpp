#include "server_rpc.hpp"
#include "protocol.hpp"
#include <array>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <string_view>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace rpc {

static void process_udp_request(int sock_fd, const protocol::Request &req, sockaddr_in client_addr,
                                socklen_t client_len) {
    protocol::Response res{};
    res.seq_num = req.seq_num;
    res.status = -1;

    if (req.auth_token == 0) {
        ::sendto(sock_fd, &res, sizeof(res), 0, reinterpret_cast<sockaddr *>(&client_addr), client_len);
        return;
    }

    auto safe_c_str = [](auto arr) {
        arr.back() = '\0';
        return arr;
    };

    switch (req.opcode) {
        case protocol::Opcode::Open: {
            auto path = safe_c_str(req.pathname);
            auto mode = safe_c_str(req.mode);
            std::string_view mode_str{mode.data()};

            int flags = O_RDONLY;
            if (mode_str == "w")
                flags = O_WRONLY | O_CREAT;
            else if (mode_str == "rw" || mode_str == "wr")
                flags = O_RDWR | O_CREAT;

            res.status = ::open(path.data(), flags, 0666);
            break;
        }
        case protocol::Opcode::Read: {
            size_t bytes_to_read = std::min(req.count, static_cast<uint64_t>(res.data.size()));
            res.status = static_cast<int32_t>(::read(req.fd, res.data.data(), bytes_to_read));
            break;
        }
        case protocol::Opcode::Seek: {
            constexpr std::array SEEK_MAP = {SEEK_SET, SEEK_CUR, SEEK_END};

            size_t whence_idx = static_cast<size_t>(req.whence);

            off_t result = ::lseek(req.fd, static_cast<off_t>(req.offset), SEEK_MAP[whence_idx]);
            if (result >= 0) {
                res.status = 0;
                res.offset_result = static_cast<int64_t>(result);
            }
            break;
        }
        case protocol::Opcode::Write: {
            size_t bytes_to_write = std::min(req.count, static_cast<uint64_t>(req.data.size()));
            res.status = static_cast<int32_t>(::write(req.fd, req.data.data(), bytes_to_write));
            break;
        }
        case protocol::Opcode::Chmod: {
            auto path = safe_c_str(req.pathname);
            res.status = ::chmod(path.data(), static_cast<mode_t>(req.file_mode));
            break;
        }
        case protocol::Opcode::Unlink: {
            auto path = safe_c_str(req.pathname);
            res.status = ::unlink(path.data());
            break;
        }
        case protocol::Opcode::Rename: {
            auto path = safe_c_str(req.pathname);
            auto new_path = safe_c_str(req.new_pathname);
            res.status = ::rename(path.data(), new_path.data());
            break;
        }
        default: {
            res.status = -1;
            break;
        }
    }

    ::sendto(sock_fd, &res, sizeof(res), 0, reinterpret_cast<sockaddr *>(&client_addr), client_len);
}

Server::Server(uint16_t port) {
    sock_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd_ < 0) {
        throw std::runtime_error("Cannot create socket");
    }

    int opt = 1;
    ::setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(sock_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        ::close(sock_fd_);
        throw std::runtime_error("Binding error. Port may be in use");
    }
}

Server::~Server() {
    if (sock_fd_ != -1) {
        ::close(sock_fd_);
    }
}

void Server::run() {
    std::cout << "Server is working ...\n";

    while (true) {
        protocol::Request req{};
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        ssize_t bytes_read =
            ::recvfrom(sock_fd_, &req, sizeof(req), 0, reinterpret_cast<sockaddr *>(&client_addr), &client_len);

        if (bytes_read != sizeof(req)) continue;

        process_udp_request(sock_fd_, req, client_addr, client_len);
    }
}

}  // namespace rpc