#include "server_rpc.hpp"
#include "protocol.hpp"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

RpcServer::RpcServer(int port) {
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr));
    std::cout << "Serwer nasłuchuje na porcie " << port << "...\n";
}

void RpcServer::run() {
    RpcRequest req;
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // serwer ufa tylko pierwszemu tokenowi, który zobaczy po uruchomieniu
    uint64_t valid_token = 0; 

    while (true) {
        ssize_t received = recvfrom(sock_fd, &req, sizeof(req), 0, (struct sockaddr*)&client_addr, &client_len);
        if (received < 0) continue;

        RpcResponse res;
        memset(&res, 0, sizeof(res));
        res.seq_num = req.seq_num;

        if (valid_token == 0) {
            valid_token = req.auth_token;
            std::cout << "[Serwer] Zarejestrowano token klienta: " << valid_token << "\n";
        }
        
        if (req.auth_token != valid_token) {
            std::cout << "[Serwer] ODRZUCONO - błędny token autoryzacji: " << req.auth_token << "\n";
            res.status = -EACCES;
            sendto(sock_fd, &res, sizeof(res), 0, (struct sockaddr*)&client_addr, client_len);
            continue;
        }

        if (req.opcode == OP_OPEN) {
            int flags = O_RDONLY; 
            if (strcmp(req.mode, "w") == 0) flags = O_WRONLY | O_CREAT | O_TRUNC;
            else if (strcmp(req.mode, "r+") == 0) flags = O_RDWR;

            int fd = open(req.pathname, flags, 0644);
            res.status = fd; 
            std::cout << "[Serwer] Otwarto plik: " << req.pathname << " (FD: " << fd << ")\n";
        } 
        else if (req.opcode == OP_READ) {
            ssize_t bytes_read = read(req.fd, res.data, req.count);
            res.status = bytes_read;
            std::cout << "[Serwer] Odczytano " << bytes_read << " bajtów z FD: " << req.fd << "\n";
        }

        sendto(sock_fd, &res, sizeof(res), 0, (struct sockaddr*)&client_addr, client_len);
    }
}