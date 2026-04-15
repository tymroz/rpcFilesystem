#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <endian.h>
#include <netinet/in.h>

namespace rpc::protocol {

constexpr uint64_t MAX_DATA_SIZE = 1024;
constexpr uint16_t MAX_PATHNAME_SIZE = 256;
constexpr uint8_t MAX_OPENMODE_SIZE = 16;

enum class Opcode : uint8_t { Open = 1, Read = 2, Seek = 3, Write = 4, Chmod = 5, Unlink = 6, Rename = 7 };

enum class SeekWhence : uint8_t { Set = 0, Cur = 1, End = 2 };

struct Request {
    uint64_t auth_token;
    uint64_t seq_num;
    Opcode opcode;

    std::array<std::byte, MAX_DATA_SIZE> data;
    std::array<char, MAX_PATHNAME_SIZE> pathname;
    std::array<char, MAX_PATHNAME_SIZE> new_pathname;
    std::array<char, MAX_OPENMODE_SIZE> mode;

    int32_t fd;
    uint64_t count;
    int64_t offset;
    uint32_t file_mode;
    SeekWhence whence;
};

struct Response {
    uint64_t seq_num;
    int32_t status;  // >= 0 succes, < 0 error

    std::array<std::byte, MAX_DATA_SIZE> data;
    int64_t offset_result;
};

inline void request_to_network(Request& req) {
    req.auth_token = htobe64(req.auth_token);
    req.seq_num = htobe64(req.seq_num);
    req.count = htobe64(req.count);
    req.offset = htobe64(static_cast<uint64_t>(req.offset));
    req.fd = htonl(req.fd);
    req.file_mode = htonl(req.file_mode);
}

inline void request_from_network(Request& req) {
    req.auth_token = be64toh(req.auth_token);
    req.seq_num = be64toh(req.seq_num);
    req.count = be64toh(req.count);
    req.offset = static_cast<int64_t>(be64toh(static_cast<uint64_t>(req.offset)));
    req.fd = ntohl(req.fd);
    req.file_mode = ntohl(req.file_mode);
}

inline void response_to_network(Response& res) {
    res.seq_num = htobe64(res.seq_num);
    res.status = htonl(res.status);
    res.offset_result = htobe64(static_cast<uint64_t>(res.offset_result));
}

inline void response_from_network(Response& res) {
    res.seq_num = be64toh(res.seq_num);
    res.status = ntohl(res.status);
    res.offset_result = static_cast<int64_t>(be64toh(static_cast<uint64_t>(res.offset_result)));
}

}  // namespace rpc::protocol