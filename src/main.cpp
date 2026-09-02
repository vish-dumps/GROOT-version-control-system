#include <iostream>
#include <string>

int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cout << "Usage: groot <command>\n";
        return 1;
    }

    std::string command = argv[1];

    if (command == "init") {
        std::cout << "Initializing Groot...\n";
    }
    else {
        std::cout << "Unknown command: " << command << '\n';
        return 1;
    }

    return 0;
}