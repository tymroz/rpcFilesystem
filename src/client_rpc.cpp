#include "client_rpc.hpp"
#include "protocol.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <stdexcept>
#include <string>
#include <sys/random.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace rpc {

static uint64_t generate_random_u64() {
    uint64_t val = 0;
    if (::getrandom(&val, sizeof(val), 0) != sizeof(val)) {
        throw std::runtime_error("Error generating random number");
    }
    return val;
}

Client::Client(std::string_view server_ip, uint16_t port) {
    sock_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd_ < 0) {
        throw std::runtime_error("Cannot create socket");
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    std::string ip_str{server_ip};
    if (::inet_pton(AF_INET, ip_str.c_str(), &server_addr.sin_addr) <= 0) {
        ::close(sock_fd_);
        throw std::runtime_error("Invalid IP address");
    }

    if (::connect(sock_fd_, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0) {
        ::close(sock_fd_);
        throw std::runtime_error("Error connecting to server");
    }

    struct timeval tv{};
    tv.tv_sec = 2;
    ::setsockopt(sock_fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    auth_token_ = generate_random_u64();
}

Client::~Client() {
    if (sock_fd_ != -1) {
        ::close(sock_fd_);
    }
}

std::optional<protocol::Response> Client::send_with_retry(protocol::Request &req) {
    req.auth_token = auth_token_;
    req.seq_num = generate_random_u64();

    const int MAX_TRIES = 2;

    for (int attempt = 0; attempt < MAX_TRIES; attempt++) {
        if (::send(sock_fd_, &req, sizeof(req), 0) != sizeof(req)) {
            continue;
        }

        protocol::Response res{};
        while (true) {
            ssize_t bytes_recv = ::recv(sock_fd_, &res, sizeof(res), 0);

            if (bytes_recv < 0) {
                break;
            }

            if (bytes_recv == sizeof(res) && res.seq_num == req.seq_num) {
                return res;
            }
        }
    }

    return std::nullopt;
}

std::optional<File> Client::open(std::string_view pathname, std::string_view mode) {
    protocol::Request req{};
    req.opcode = protocol::Opcode::Open;

    auto path_len = std::min(pathname.size(), req.pathname.size() - 1);
    std::copy_n(pathname.begin(), path_len, req.pathname.begin());

    auto mode_len = std::min(mode.size(), req.mode.size() - 1);
    std::copy_n(mode.begin(), mode_len, req.mode.begin());

    auto res_opt = send_with_retry(req);

    if (res_opt && res_opt->status >= 0) {
        return File{res_opt->status};
    }

    return std::nullopt;
}

std::ptrdiff_t Client::read(const File &file, std::span<std::byte> buffer) {
    protocol::Request req{};
    req.opcode = protocol::Opcode::Read;
    req.fd = file.fd();

    constexpr uint64_t MAX_DATA = 1024;
    req.count = std::min(static_cast<uint64_t>(buffer.size()), MAX_DATA);

    auto res_opt = send_with_retry(req);

    if (res_opt->status > 0) {
        std::copy_n(res_opt->data.begin(), res_opt->status, buffer.begin());
    }

    return res_opt->status;
}

std::optional<int64_t> Client::seek(const File &file, int64_t offset, protocol::SeekWhence whence) {
    protocol::Request req{};
    req.opcode = protocol::Opcode::Seek;
    req.fd = file.fd();
    req.offset = offset;
    req.whence = whence;

    auto res_opt = send_with_retry(req);

    if (res_opt->status < 0) {
        return std::nullopt;
    }

    return res_opt->offset_result;
}

std::ptrdiff_t Client::write(const File &file, std::span<const std::byte> buffer) {
    protocol::Request req{};
    req.opcode = protocol::Opcode::Write;
    req.fd = file.fd();

    constexpr uint64_t MAX_DATA = 1024;
    req.count = std::min(static_cast<uint64_t>(buffer.size()), MAX_DATA);

    std::copy_n(buffer.begin(), req.count, req.data.begin());

    auto res_opt = send_with_retry(req);

    if (res_opt && res_opt->status >= 0) {
        return res_opt->status;
    }

    return -1;
}

}  // namespace rpc