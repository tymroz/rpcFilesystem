#include "client_rpc.hpp"
#include <array>
#include <cstdint>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <unordered_map>

void print_menu() {
    std::cout << "╔════════════════════════════════════════════════╗\n";
    std::cout << "║ 1. open <path> <mode>        - Open file       ║\n";
    std::cout << "║ 2. read <bytes>              - Read data       ║\n";
    std::cout << "║ 3. write <text>              - Write data      ║\n";
    std::cout << "║ 4. seek <offset> <whence>    - Seek position   ║\n";
    std::cout << "║    whence: 0=SET, 1=CUR, 2=END                 ║\n";
    std::cout << "║ 5. close                     - Close file      ║\n";
    std::cout << "║ 6. chmod <path> <mode>       - Change perms    ║\n";
    std::cout << "║ 7. unlink <path>             - Delete file     ║\n";
    std::cout << "║ 8. rename <old> <new>        - Rename file     ║\n";
    std::cout << "║ 9. help                      - Show help       ║\n";
    std::cout << "║ 10. quit                     - Exit program    ║\n";
    std::cout << "╚════════════════════════════════════════════════╝\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <ip> <port>\n";
        return 1;
    }

    try {
        uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));
        rpc::Client client(argv[1], port);
        std::cout << "Connected to server at " << argv[1] << ":" << port << "\n";

        std::optional<rpc::File> current_file;

        print_menu();

        auto require_file = [&]() -> bool {
            if (!current_file.has_value()) {
                std::cout << "Error: No file open. Use 'open <path> <mode>' first.\n";
                return false;
            }
            return true;
        };

        using CommandHandler = std::function<void(std::istringstream&)>;
        std::unordered_map<std::string, CommandHandler> commands;

        commands["open"] = [&](std::istringstream& iss) {
            std::string pathname, mode;
            if (!(iss >> pathname >> mode)) {
                std::cout << "Usage: open <path> <mode>\n";
                return;
            }

            current_file.reset();
            if (auto file_opt = client.open(pathname, mode)) {
                current_file = file_opt;
                std::cout << "File opened successfully. FD: " << current_file->fd() << "\n";
            } else {
                std::cout << "Error: Failed to open file.\n";
            }
        };

        commands["read"] = [&](std::istringstream& iss) {
            std::size_t count;
            if (!(iss >> count)) {
                std::cout << "Usage: read <bytes>\n";
                return;
            }
            if (!require_file()) return;

            if (count > 1024) {
                std::cout << "Max read size: 1024 bytes. Reducing to 1024.\n";
                count = 1024;
            }

            std::array<std::byte, 1024> buffer{};
            auto bytes_read = client.read(*current_file, std::span{buffer.data(), count});

            if (bytes_read < 0) {
                std::cout << "Error during file read.\n";
            } else if (bytes_read == 0) {
                std::cout << "End of file or 0 bytes read.\n";
            } else {
                std::cout << "Read " << bytes_read << " bytes.\n--- CONTENT ---\n";
                std::string_view content{reinterpret_cast<const char*>(buffer.data()),
                                         static_cast<std::size_t>(bytes_read)};
                std::cout << content << "\n---------------\n";
            }
        };

        commands["seek"] = [&](std::istringstream& iss) {
            int64_t offset;
            int whence_int;
            if (!(iss >> offset >> whence_int)) {
                std::cout << "Usage: seek <offset> <whence>\n" << "  whence: 0=SET, 1=CUR, 2=END\n";
                return;
            }
            if (whence_int < 0 || whence_int > 2) {
                std::cout << "Error: whence must be 0 (SET), 1 (CUR), or 2 (END).\n";
                return;
            }
            if (!require_file()) return;

            auto whence = static_cast<rpc::protocol::SeekWhence>(whence_int);
            if (auto new_pos = client.seek(*current_file, offset, whence)) {
                std::cout << "New file position: " << new_pos.value() << " bytes.\n";
            } else {
                std::cout << "Error during seek operation.\n";
            }
        };

        commands["write"] = [&](std::istringstream& iss) {
            if (!require_file()) return;

            std::string text;
            std::getline(iss >> std::ws, text);

            if (text.empty()) {
                std::cout << "Usage: write <text>\n";
                return;
            }

            if (text.size() > 1024) {
                std::cout << "Text too long. Truncating to 1024 bytes.\n";
                text.resize(1024);
            }

            auto data_ptr = reinterpret_cast<const std::byte*>(text.data());
            auto bytes_written = client.write(*current_file, std::span{data_ptr, text.size()});

            if (bytes_written < 0)
                std::cout << "Error during file write.\n";
            else
                std::cout << "Wrote " << bytes_written << " bytes.\n";
        };

        commands["close"] = [&](std::istringstream&) {
            if (current_file) {
                current_file.reset();
                std::cout << "File closed.\n";
            } else {
                std::cout << "No file open.\n";
            }
        };

        commands["chmod"] = [&](std::istringstream& iss) {
            std::string pathname;
            uint32_t mode;
            if (!(iss >> pathname >> std::oct >> mode >> std::dec)) {
                std::cout << "Usage: chmod <path> <mode>\n";
                return;
            }
            if (client.chmod(pathname, mode))
                std::cout << "Permissions changed.\n";
            else
                std::cout << "Error: Failed to change permissions.\n";
        };

        commands["unlink"] = [&](std::istringstream& iss) {
            std::string pathname;
            if (!(iss >> pathname)) {
                std::cout << "Usage: unlink <path>\n";
                return;
            }
            if (client.unlink(pathname))
                std::cout << "File deleted.\n";
            else
                std::cout << "Error: Failed to delete file.\n";
        };

        commands["rename"] = [&](std::istringstream& iss) {
            std::string oldpath, newpath;
            if (!(iss >> oldpath >> newpath)) {
                std::cout << "Usage: rename <oldpath> <newpath>\n";
                return;
            }
            if (client.rename(oldpath, newpath))
                std::cout << "File renamed.\n";
            else
                std::cout << "Error: Failed to rename file.\n";
        };

        commands["help"] = [&](std::istringstream&) { print_menu(); };

        while (true) {
            std::cout << "\n> ";

            std::string line;
            if (!std::getline(std::cin, line)) break;

            if (line.empty()) continue;

            std::istringstream iss(line);
            std::string command;
            iss >> command;

            if (command == "quit") {
                std::cout << "Closing client...\n";
                break;
            }

            if (auto it = commands.find(command); it != commands.end()) {
                it->second(iss);
            } else if (!command.empty()) {
                std::cout << "Unknown command: '" << command << "'\n" << "Type 'help' to see available commands.\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "\n[CLIENT ERROR]: " << e.what() << '\n';
        return 1;
    }
    return 0;
}