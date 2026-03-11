#pragma once

class RpcServer {
public:
    RpcServer(int port);
    void run();
private:
    int sock_fd;
};