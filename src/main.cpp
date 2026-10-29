#include "groot/repository.hpp"

#include <exception>
#include <iostream>
#include <string_view>

namespace {

void print_status(const groot::Status& status) {
    std::cout << "On branch " << status.branch << "\n";
    if (!status.staged.empty()) {
        std::cout << "\nChanges staged for commit:\n";
        for (const auto& path : status.staged) {
            std::cout << "    new file: " << path << '\n';
        }
    }
    if (!status.modified.empty() || !status.deleted.empty()) {
        std::cout << "\nChanges not staged:\n";
        for (const auto& path : status.modified) {
            std::cout << "    modified: " << path << '\n';
        }
        for (const auto& path : status.deleted) {
            std::cout << "    deleted:  " << path << '\n';
        }
    }
    if (!status.untracked.empty()) {
        std::cout << "\nUntracked files:\n";
        for (const auto& path : status.untracked) {
            std::cout << "    " << path << '\n';
        }
    }
    if (status.staged.empty() && status.modified.empty() &&
        status.deleted.empty() && status.untracked.empty()) {
        std::cout << "\nnothing to commit, working tree clean\n";
    }
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: groot <init|add|status> [path]\n";
        return 2;
    }

    try {
        const std::string_view command(argv[1]);
        if (command == "init" && argc == 2) {
            const auto metadata = groot::Repository::init(std::filesystem::current_path());
            std::cout << "Initialized empty Groot repository in " << metadata.string() << '\n';
        } else if (command == "add" && argc == 3) {
            const auto count = groot::Repository::add(argv[2]);
            std::cout << "Updated index: " << count << " path(s)\n";
        } else if (command == "status" && argc == 2) {
            print_status(groot::Repository::status());
        } else {
            std::cerr << "Usage: groot <init|add|status> [path]\n";
            return 2;
        }
        return 0;
    } catch (const std::exception& error) {
        const std::string_view message(error.what());
        if (message.starts_with("fatal:")) {
            std::cerr << message << '\n';
        } else {
            std::cerr << "error: " << message << '\n';
        }
        return 1;
    }
}
