#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

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

}  // namespace rpc::protocol