#include "client_rpc.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <limits>

void print_menu() {
    std::cout << "\n=======================\n";
    std::cout << "  open <sciezka> <tryb>  (np. open test.txt r)\n";
    std::cout << "  read <liczba_bajtow>\n";
    std::cout << "  exit | quit\n";
    std::cout << "========================\n";
}

int main() {
    rpc_init("127.0.0.1", 8080);

    RemoteFile* current_file = nullptr;
    std::string command;

    print_menu();

    while (true) {
        std::cout << "> ";
        std::cin >> command;

        if (command == "exit" || command == "quit") {
            break;
        } 
        else if (command == "open") {
            std::string filename, mode;
            std::cin >> filename >> mode;

            if (std::cin.fail()) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "[Błąd] Niewłaściwe argumenty. Użycie: open <sciezka> <tryb>\n";
                continue;
            }

            if (current_file) {
                delete current_file;
                current_file = nullptr;
            }

            std::cout << "Wysyłanie żądania open...\n";
            current_file = rpc_open(filename.c_str(), mode.c_str());

            if (current_file) {
                std::cout << "[Sukces] Plik '" << filename << "' otwarty pomyślnie.\n";
            } else {
                std::cout << "[Błąd] Nie udało się otworzyć pliku na serwerze (odmowa dostępu lub plik nie istnieje).\n";
            }
        } 
        else if (command == "read") {
            size_t count;
            std::cin >> count;

            if (std::cin.fail()) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "[Błąd] Argument musi być liczbą. Użycie: read <liczba>\n";
                continue;
            }

            if (!current_file) {
                std::cout << "[Błąd] Brak otwartego pliku. Użyj najpierw komendy 'open'.\n";
                continue;
            }

            std::cout << "Wysyłanie żądania read na " << count << " bajtów...\n";
            
            std::vector<char> buffer(count + 1, 0);

            ssize_t bytes = rpc_read(current_file, buffer.data(), count);
            
            if (bytes >= 0) {
                buffer[bytes] = '\0'; 
                std::cout << "[Sukces] Odczytano " << bytes << " bajtów.\n";
                std::cout << "Zawartość:\n" << buffer.data() << "\n";
            } else {
                std::cout << "[Błąd] Odczyt nie powiódł się (timeout, błąd na serwerze lub koniec pliku).\n";
            }
        } 
        else {
            std::cout << "[Błąd] Nieznana komenda: " << command << "\n";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            print_menu();
        }
    }

    std::cout << "Zamykanie klienta...\n";
    if (current_file) {
        delete current_file;
    }

    return 0;
}