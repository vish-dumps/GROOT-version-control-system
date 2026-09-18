#include <iostream>
#include <string>

#include "Repository.h"

int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cout << "Usage: groot <command>\n";
        return 1;
    }

    std::string command = argv[1];

    Repository repo;

    if (command == "init") {

        repo.init();

    } else if (command == "status") {

        repo.status();

    } else if (command == "add") {

        if (argc < 3) {
            std::cout << "Usage: groot add <file>\n";
            return 1;
        }

        repo.add(argv[2]);



    }

    else if (command == "commit") {

        if (argc < 4 || std::string(argv[2]) != "-m") {
            std::cout << "Usage: groot commit -m \"message\"\n";
            return 1;
        }

        repo.commit(argv[3]);

    }
    else if (command == "log") {
        repo.log();
    }

    else {

        std::cout << "Unknown command: " << command << '\n';
        return 1;
    }

    return 0;
}