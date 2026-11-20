#include "groot/repository.hpp"

#include <exception>
#include <iostream>
#include <string_view>

namespace {

void print_status(const groot::Status& status) {
    if (status.detached) std::cout << "HEAD detached at " << status.branch << "\n";
    else std::cout << "On branch " << status.branch << "\n";
    if (status.merging) std::cout << "Merge in progress; resolve conflicts and stage files before committing.\n";
    if (!status.staged.empty()) {
        std::cout << "\nChanges staged for commit:\n";
        for (const auto& path : status.staged) {
            std::cout << "    new file: " << path << '\n';
        }
    }
    if (!status.staged_modified.empty() || !status.staged_deleted.empty()) {
        if (status.staged.empty()) std::cout << "\nChanges staged for commit:\n";
        for (const auto& path : status.staged_modified) std::cout << "    modified: " << path << '\n';
        for (const auto& path : status.staged_deleted) std::cout << "    deleted:  " << path << '\n';
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
    if (!status.merging && status.staged.empty() && status.staged_modified.empty() && status.staged_deleted.empty() && status.modified.empty() &&
        status.deleted.empty() && status.untracked.empty()) {
        std::cout << "\nnothing to commit, working tree clean\n";
    }
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: groot <init|add|status|diff|commit|log|branch|checkout|switch|merge> [args]\n";
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
        } else if (command == "commit" && argc == 4 && std::string_view(argv[2]) == "-m") {
            std::cout << "Committed " << groot::Repository::commit(argv[3]) << '\n';
        } else if (command == "log" && (argc == 2 || (argc == 3 && std::string_view(argv[2]) == "--all"))) {
            const auto history = argc == 2 ? groot::Repository::log() : groot::Repository::log_all();
            for (const auto& record : history) {
                std::cout << "commit " << record.hash << "\n    " << record.message << "\n\n";
            }
        } else if (command == "log" && argc == 3 && std::string_view(argv[2]) == "--graph") {
            std::cout << groot::Repository::graph();
        } else if (command == "diff" && (argc == 2 || (argc == 3 && std::string_view(argv[2]) == "--staged"))) {
            std::cout << groot::Repository::diff(argc == 3);
        } else if (command == "branch" && argc == 2) {
            const auto active = groot::Repository::status().branch;
            for (const auto& name : groot::Repository::branches())
                std::cout << (name == active ? "* " : "  ") << name << '\n';
        } else if (command == "branch" && argc == 3) {
            groot::Repository::create_branch(argv[2]);
        } else if (command == "checkout" && argc == 3) {
            groot::Repository::checkout(argv[2]);
            const auto state = groot::Repository::status();
            if (state.detached) std::cout << "HEAD detached at " << state.branch << '\n';
            else std::cout << "Switched to branch '" << state.branch << "'\n";
        } else if (command == "switch" && argc == 3) {
            groot::Repository::switch_branch(argv[2]);
            std::cout << "Switched to branch '" << argv[2] << "'\n";
        } else if (command == "merge" && argc == 3 && std::string_view(argv[2]) == "--abort") {
            groot::Repository::abort_merge();
            std::cout << "Merge aborted.\n";
        } else if (command == "merge" && argc == 3) {
            const auto result = groot::Repository::merge(argv[2]);
            switch (result.kind) {
            case groot::MergeResult::Kind::AlreadyUpToDate: std::cout << "Already up to date.\n"; break;
            case groot::MergeResult::Kind::FastForward: std::cout << "Fast-forward to " << result.hash << '\n'; break;
            case groot::MergeResult::Kind::Merged: std::cout << "Merge commit " << result.hash << '\n'; break;
            case groot::MergeResult::Kind::Conflicts:
                for (const auto& path : result.conflicts) std::cout << "CONFLICT: " << path << '\n';
                std::cout << "Resolve conflicts, stage files, then commit.\n";
                break;
            }
        } else {
            std::cerr << "Usage: groot <init|add|status|diff|commit|log|branch|checkout|switch|merge> [args]\n";
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
