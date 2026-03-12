#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace rpc::protocol {

enum class Opcode : uint8_t { Open = 1, Read = 2, Seek = 3, Write = 4 };

enum class SeekWhence : uint8_t { Set = 0, Cur = 1, End = 2 };

struct Request {
    uint64_t auth_token;
    uint64_t seq_num;
    Opcode opcode;

    std::array<std::byte, 1024> data;
    std::array<char, 256> pathname;
    std::array<char, 16> mode;

    int32_t fd;
    uint64_t count;
    int64_t offset;
    SeekWhence whence;
};

struct Response {
    uint64_t seq_num;
    int32_t status;  // >= 0 sukces, < 0 błąd

    std::array<std::byte, 1024> data;
    int64_t offset_result;
};

}  // namespace rpc::protocol