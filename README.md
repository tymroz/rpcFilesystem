# RPC Filesystem

UDP-based RPC filesystem implementation in C++20 with client-server architecture. Provides remote file operations (open, read, write, seek, chmod, unlink, rename) over UDP sockets.

## Build

```bash
cmake -B build
cmake --build build
```

This produces:
- `client_lib` - Static library
- `client_app` - Client executable  
- `server_app` - Server executable

## Run

**Server:**
```bash
./build/server_app <port>
```

**Client:**
```bash
./build/client_app <server ip> <port>
```
