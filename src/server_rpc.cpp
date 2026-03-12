#include "server_rpc.hpp"
#include "protocol.hpp"
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <string_view>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>

namespace rpc {

static void process_udp_request(int sock_fd, protocol::Request req, sockaddr_in client_addr, socklen_t client_len) {
    protocol::Response res{};
    res.seq_num = req.seq_num;
    res.status = -1;

    // a bit too simple auth: accept every non-zero token
    if (req.auth_token == 0) {
        res.status = -2;  // auth error
    } else {
        if (req.opcode == protocol::Opcode::Open) {
            req.pathname.back() = '\0';
            req.mode.back() = '\0';

            std::string_view mode_str{req.mode.data()};
            int flags = O_RDONLY;
            if (mode_str == "w")
                flags = O_WRONLY | O_CREAT | O_TRUNC;
            else if (mode_str == "rw")
                flags = O_RDWR | O_CREAT;

            int fd = ::open(req.pathname.data(), flags, 0666);
            res.status = fd;
        } else if (req.opcode == protocol::Opcode::Read) {
            size_t bytes_to_read = std::min(req.count, static_cast<uint64_t>(res.data.size()));
            ssize_t bytes_read_file = ::read(req.fd, res.data.data(), bytes_to_read);
            res.status = static_cast<int32_t>(bytes_read_file);
        } else if (req.opcode == protocol::Opcode::Seek) {
            int posix_whence = SEEK_SET;
            switch (req.whence) {
                case protocol::SeekWhence::Set:
                    posix_whence = SEEK_SET;
                    break;
                case protocol::SeekWhence::Cur:
                    posix_whence = SEEK_CUR;
                    break;
                case protocol::SeekWhence::End:
                    posix_whence = SEEK_END;
                    break;
            }

            off_t result = ::lseek(req.fd, static_cast<off_t>(req.offset), posix_whence);
            if (result < 0) {
                res.status = -1;
            } else {
                res.status = 0;
                res.offset_result = static_cast<int64_t>(result);
            }
        } else if (req.opcode == protocol::Opcode::Write) {
            size_t bytes_to_write = std::min(req.count, static_cast<uint64_t>(req.data.size()));
            ssize_t bytes_written = ::write(req.fd, req.data.data(), bytes_to_write);
            res.status = static_cast<int32_t>(bytes_written);
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

        std::thread worker(process_udp_request, sock_fd_, req, client_addr, client_len);
        worker.detach();
    }
}

}  // namespace rpc