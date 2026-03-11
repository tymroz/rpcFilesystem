#include "client_rpc.hpp"
#include "protocol.hpp"
#include <iostream>
#include <cstring>
#include <random>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

static int sock_fd;
static sockaddr_in server_addr;
static uint64_t current_auth_token;
static std::mt19937_64 rng(std::random_device{}());
static uint64_t current_seq_num = 1;

void rpc_init(const char* server_ip, int port) {
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    current_auth_token = rng();
}

bool send_and_receive(RpcRequest& req, RpcResponse& res) {
    req.auth_token = current_auth_token;
    // req.seq_num = rng();
    req.seq_num = current_seq_num;
    current_seq_num++;

    for (int attempt = 0; attempt < 2; attempt++) {
        sendto(sock_fd, &req, sizeof(req), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock_fd, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;

        int ret = select(sock_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (ret > 0) {
            socklen_t len = sizeof(server_addr);
            recvfrom(sock_fd, &res, sizeof(res), 0, (struct sockaddr*)&server_addr, &len);
            
            if (res.seq_num == req.seq_num) {
                return true;
            }
        } else {
            std::cout << "Timeout żądania, próba " << attempt + 1 << "/2...\n";
        }
    }
    return false;
}

RemoteFile *rpc_open(const char *pathname, const char *mode) {
    RpcRequest req;
    memset(&req, 0, sizeof(req));
    req.opcode = OP_OPEN;
    strncpy(req.pathname, pathname, sizeof(req.pathname) - 1);
    strncpy(req.mode, mode, sizeof(req.mode) - 1);

    RpcResponse res;
    if (send_and_receive(req, res) && res.status >= 0) {
        RemoteFile* file = new RemoteFile;
        file->server_fd = res.status;
        return file;
    }
    return nullptr;
}

ssize_t rpc_read(RemoteFile *file, void *buf, size_t count) {
    if (!file) return -1;
    
    RpcRequest req;
    memset(&req, 0, sizeof(req));
    req.opcode = OP_READ;
    req.fd = file->server_fd;
    req.count = count > sizeof(RpcResponse::data) ? sizeof(RpcResponse::data) : count;

    RpcResponse res;
    if (send_and_receive(req, res) && res.status >= 0) {
        memcpy(buf, res.data, res.status);
        return res.status;
    }
    return -1;
}