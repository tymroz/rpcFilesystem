#pragma once
#include <cstddef>
#include <sys/types.h>

struct RemoteFile {
    int server_fd;
};

void rpc_init(const char* server_ip, int port);

RemoteFile *rpc_open(const char *pathname, const char *mode);
ssize_t rpc_read(RemoteFile *file, void *buf, size_t count);