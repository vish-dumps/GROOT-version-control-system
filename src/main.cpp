#include <iostream>
#include <string>

#include "Repository.h"

int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cout << "Usage: groot <command>\n";
        return 1;
    }

    std::string command = argv[1];

    if (command == "init") {

        Repository repo;
        repo.init();

    } else {

        std::cout << "Unknown command: " << command << '\n';
        return 1;
    }

    return 0;
}