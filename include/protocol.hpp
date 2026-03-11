#pragma once
#include <cstdint>
#include <cstddef>

enum RpcOpcode {
    OP_OPEN = 1,
    OP_READ = 2
};

struct RpcRequest {
    uint64_t auth_token;
    uint64_t seq_num;
    RpcOpcode opcode;
    
    char pathname[256];
    char mode[16];
    int fd;
    size_t count;
};

struct RpcResponse {
    uint64_t seq_num;
    int status; // >= 0 sukces, < 0 błąd
    char data[1024];
};