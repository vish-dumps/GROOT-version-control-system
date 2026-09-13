#include "Repository.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;


//finding the nearest .groot directory

fs::path Repository::findRepositoryRoot() {

    fs::path current = fs::current_path();

    while (true) {

        if (fs::exists(current / ".groot") &&
            fs::is_directory(current / ".groot")) {

            return current;
            }

        if (current == current.root_path()) {
            break;
        }

        current = current.parent_path();
    }

    return {};
}


// inittializing a .groot folder with all the prereq for VCS

// void Repository::init() {
//
//     fs::path repo = ".groot";
//
//     if (fs::exists(repo)) {
//
//         if (fs::is_directory(repo)) {
//             std::cout << "Groot repository already exists.\n";
//         } else {
//             std::cout << "Error: .groot exists but is not a directory.\n";
//         }
//
//         return;
//     }
//
//     try {
//
//         fs::create_directories(repo / "objects");
//         fs::create_directories(repo / "refs");
//
//         std::ofstream head(repo / "HEAD");
//         std::ofstream index(repo / "index");
//
//         if (!head || !index) {
//             std::cout << "Error: Could not create repository files.\n";
//             return;
//         }
//
//         std::cout << "Initialized empty Groot repository.\n";
//
//     } catch (const fs::filesystem_error& e) {
//
//         std::cout << "Filesystem error: "
//                   << e.what() << '\n';
//     }
// }


void Repository::init() {

    fs::path root = findRepositoryRoot();

    if (!root.empty()) {
        std::cout << "Repository found at: "
                  << root << '\n';
        return;
    }

    std::cout << "No Groot repository found.\n";

    fs::path repo = ".groot";

    if (fs::exists(repo)) {

        if (fs::is_directory(repo)) {
            std::cout << "Groot repository already exists.\n";
        } else {
            std::cout << "Error: .groot exists but is not a directory.\n";
        }

        return;
    }

    try {

        fs::create_directories(repo / "objects");
        fs::create_directories(repo / "refs");

        std::ofstream head(repo / "HEAD");
        std::ofstream index(repo / "index");

        if (!head || !index) {
            std::cout << "Error: Could not create repository files.\n";
            return;
        }

        std::cout << "Initialized empty Groot repository.\n";

    } catch (const fs::filesystem_error& e) {

        std::cout << "Filesystem error: "
                  << e.what() << '\n';
    }
}

