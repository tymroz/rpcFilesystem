#include "client_rpc.hpp"
#include "protocol.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <stdexcept>
#include <sys/random.h>
#include <sys/socket.h>
#include <unistd.h>

namespace rpc {

static uint64_t generate_random_u64() {
    uint64_t val = 0;
    if (::getrandom(&val, sizeof(val), 0) != sizeof(val)) {
        throw std::runtime_error("Error generating random number");
    }
    return val;
}

template <size_t N>
static void fill_buffer(std::array<char, N>& dest, std::string_view src) {
    const size_t len = std::min(src.size(), N - 1);
    std::copy_n(src.begin(), len, dest.begin());
}

Client::Client(std::string_view server_ip, uint16_t port) {
    sock_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd_ < 0) {
        throw std::runtime_error("Cannot create socket");
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (::inet_pton(AF_INET, server_ip.data(), &server_addr.sin_addr) <= 0) {
        ::close(sock_fd_);
        throw std::runtime_error("Invalid IP address");
    }

    if (::connect(sock_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        ::close(sock_fd_);
        throw std::runtime_error("Error connecting to server");
    }

    struct timeval tv{.tv_sec = 2, .tv_usec = 0};
    ::setsockopt(sock_fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    auth_token_ = generate_random_u64();
}

Client::~Client() {
    if (sock_fd_ != -1) {
        ::close(sock_fd_);
    }
}

std::optional<protocol::Response> Client::send_with_retry(protocol::Request& req) {
    req.auth_token = auth_token_;
    req.seq_num = ++seq_num_;

    protocol::Request req_network = req;
    protocol::request_to_network(req_network);

    for (int attempt = 0; attempt < 2; ++attempt) {
        if (::send(sock_fd_, &req_network, sizeof(req_network), 0) != sizeof(req_network)) {
            continue;
        }

        protocol::Response res{};
        while (true) {
            ssize_t bytes_recv = ::recv(sock_fd_, &res, sizeof(res), 0);

            if (bytes_recv < 0) break;

            if (bytes_recv == sizeof(res)) {
                protocol::response_from_network(res);
                if (res.seq_num == req.seq_num) {
                    return res;
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<File> Client::open(std::string_view pathname, std::string_view mode) {
    protocol::Request req{};
    req.opcode = protocol::Opcode::Open;
    fill_buffer(req.pathname, pathname);
    fill_buffer(req.mode, mode);

    auto res = send_with_retry(req);
    return (res && res->status >= 0) ? std::make_optional<File>(res->status) : std::nullopt;
}

std::ptrdiff_t Client::read(const File& file, std::span<std::byte> buffer) {
    protocol::Request req{};
    req.opcode = protocol::Opcode::Read;
    req.fd = file.fd();
    req.count = std::min<uint64_t>(buffer.size(), protocol::MAX_DATA_SIZE);

    auto res = send_with_retry(req);
    if (!res || res->status < 0) return -1;

    if (res->status > 0) {
        std::copy_n(res->data.begin(), res->status, buffer.begin());
    }
    return res->status;
}

std::optional<int64_t> Client::seek(const File& file, int64_t offset, protocol::SeekWhence whence) {
    protocol::Request req{};
    req.opcode = protocol::Opcode::Seek;
    req.fd = file.fd();
    req.offset = offset;
    req.whence = whence;

    auto res = send_with_retry(req);
    return (res && res->status >= 0) ? std::make_optional(res->offset_result) : std::nullopt;
}

std::ptrdiff_t Client::write(const File& file, std::span<const std::byte> buffer) {
    protocol::Request req{};
    req.opcode = protocol::Opcode::Write;
    req.fd = file.fd();
    req.count = std::min<uint64_t>(buffer.size(), protocol::MAX_DATA_SIZE);

    std::copy_n(buffer.begin(), req.count, req.data.begin());

    auto res = send_with_retry(req);
    return (res && res->status >= 0) ? res->status : -1;
}

bool Client::chmod(std::string_view pathname, uint32_t mode) {
    protocol::Request req{};
    req.opcode = protocol::Opcode::Chmod;
    req.file_mode = mode;
    fill_buffer(req.pathname, pathname);

    auto res = send_with_retry(req);
    return res && res->status == 0;
}

bool Client::unlink(std::string_view pathname) {
    protocol::Request req{};
    req.opcode = protocol::Opcode::Unlink;
    fill_buffer(req.pathname, pathname);

    auto res = send_with_retry(req);
    return res && res->status == 0;
}

bool Client::rename(std::string_view oldpath, std::string_view newpath) {
    protocol::Request req{};
    req.opcode = protocol::Opcode::Rename;
    fill_buffer(req.pathname, oldpath);
    fill_buffer(req.new_pathname, newpath);

    auto res = send_with_retry(req);
    return res && res->status == 0;
}

}  // namespace rpc