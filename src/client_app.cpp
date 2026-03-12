#include "client_rpc.hpp"
#include <exception>
#include <limits>
#include <string>

void print_menu() {
    std::cout << "\n╔═══════════════════════════════════════════╗\n";
    std::cout << "║         RPC Filesystem - Menu             ║\n";
    std::cout << "╠═══════════════════════════════════════════╣\n";
    std::cout << "║ 1. open <path> <mode>     - Open file     ║\n";
    std::cout << "║ 2. read <bytes>           - Read data     ║\n";
    std::cout << "║ 3. write <text>           - Write data    ║\n";
    std::cout << "║ 4. seek <offset> <whence> - Seek position ║\n";
    std::cout << "║    whence: 0=SET, 1=CUR, 2=END            ║\n";
    std::cout << "║ 5. close                  - Close file    ║\n";
    std::cout << "║ 6. help                   - Show help     ║\n";
    std::cout << "║ 7. exit / quit            - Exit program  ║\n";
    std::cout << "╚═══════════════════════════════════════════╝\n";
}

int main() {
    try {
        rpc::Client client("127.0.0.1", 8080);
        std::cout << "Connected to server at 127.0.0.1:8080\n";

        std::optional<rpc::File> current_file;
        std::string command;

        print_menu();

        while (true) {
            std::cout << "\n> ";
            std::cin >> command;

            if (command == "exit" || command == "quit") {
                std::cout << "Closing client...\n";
                break;
            } else if (command == "open") {
                std::string pathname, mode;
                std::cin >> pathname >> mode;

                if (std::cin.fail()) {
                    std::cin.clear();
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    std::cout << "Usage: open <path> <mode>\n";
                    continue;
                }

                current_file.reset();

                auto file_opt = client.open(pathname, mode);

                if (file_opt.has_value()) {
                    current_file = file_opt;
                    std::cout << "File opened successfully. FD: " << current_file->fd() << "\n";
                } else {
                    std::cout << "Error: Failed to open file.\n";
                }
            } else if (command == "read") {
                std::size_t count;
                std::cin >> count;

                if (std::cin.fail()) {
                    std::cin.clear();
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    std::cout << "Usage: read <bytes>\n";
                    continue;
                }

                if (!current_file.has_value()) {
                    std::cout << "Error: No file open. Use 'open <path> <mode>' first.\n";
                    continue;
                }

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
                    std::cout << "Read " << bytes_read << " bytes.\n";
                    std::cout << "--- CONTENT ---\n";

                    std::string_view content{reinterpret_cast<const char *>(buffer.data()),
                                             static_cast<std::size_t>(bytes_read)};
                    std::cout << content << "\n";
                    std::cout << "---------------\n";
                }
            } else if (command == "seek") {
                int64_t offset;
                int whence_int;
                std::cin >> offset >> whence_int;

                if (std::cin.fail()) {
                    std::cin.clear();
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    std::cout << "Usage: seek <offset> <whence>\n";
                    std::cout << "  whence: 0=SET (from start), 1=CUR (from current), "
                                 "2=END (from end)\n";
                    continue;
                }

                if (whence_int < 0 || whence_int > 2) {
                    std::cout << "Error: whence must be 0 (SET), 1 (CUR), or 2 (END).\n";
                    continue;
                }

                if (!current_file.has_value()) {
                    std::cout << "Error: No file open. Use 'open <path> <mode>' first.\n";
                    continue;
                }

                auto whence = static_cast<rpc::protocol::SeekWhence>(whence_int);

                auto new_pos = client.seek(*current_file, offset, whence);

                if (new_pos.has_value()) {
                    std::cout << "New file position: " << new_pos.value() << " bytes.\n";
                } else {
                    std::cout << "Error during seek operation.\n";
                }
            } else if (command == "write") {
                if (!current_file.has_value()) {
                    std::cout << "Error: No file open. Use 'open <path> <mode>' first.\n";
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    continue;
                }

                std::string text;
                std::getline(std::cin >> std::ws, text);

                if (std::cin.fail()) {
                    std::cin.clear();
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    std::cout << "Usage: write <text>\n";
                    continue;
                }

                if (text.empty()) {
                    std::cout << "Error: Empty text.\n";
                    continue;
                }

                if (text.size() > 1024) {
                    std::cout << "Text too long. Truncating to 1024 bytes.\n";
                    text.resize(1024);
                }

                auto data_ptr = reinterpret_cast<const std::byte *>(text.data());
                auto bytes_written = client.write(*current_file, std::span{data_ptr, text.size()});

                if (bytes_written < 0) {
                    std::cout << "Error during file write.\n";
                } else {
                    std::cout << "Wrote " << bytes_written << " bytes.\n";
                }
            } else if (command == "close") {
                if (current_file.has_value()) {
                    current_file.reset();
                    std::cout << "File closed.\n";
                } else {
                    std::cout << "No file open.\n";
                }
            } else if (command == "help") {
                print_menu();
            } else {
                std::cout << "Unknown command: '" << command << "'\n";
                std::cout << "Type 'help' to see available commands.\n";
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }
        }

    } catch (const std::exception &e) {
        std::cerr << "\n[CLIENT ERROR]: " << e.what() << '\n';
        return 1;
    }

    return 0;
}