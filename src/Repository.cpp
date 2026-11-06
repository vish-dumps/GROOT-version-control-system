#include "groot/repository.hpp"

#include "groot/index.hpp"
#include "groot/object_store.hpp"
#include "groot/sha256.hpp"
#include "groot/utils.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace fs = std::filesystem;

namespace groot {
namespace {

std::atomic<std::uint64_t> commit_temporary_sequence{0};

fs::path require_root(const fs::path& current_directory) {
    const auto root = Repository::discover(current_directory);
    if (!root) {
        throw std::runtime_error("fatal: not a Groot repository");
    }
    return fs::canonical(*root);
}

bool is_metadata(const fs::path& path) {
    for (const auto& component : path) {
        if (component == ".groot" || component == ".git") {
            return true;
        }
    }
    return false;
}

fs::path relative_to_root(const fs::path& root, const fs::path& path) {
    const fs::path relative = fs::weakly_canonical(path).lexically_relative(root);
    if (relative.empty() || relative.is_absolute() ||
        *relative.begin() == ".." || is_metadata(relative)) {
        throw std::runtime_error("path is outside versioned files: " + path.string());
    }
    return relative;
}

std::vector<fs::path> working_files(const fs::path& directory) {
    std::vector<fs::path> files;
    for (fs::recursive_directory_iterator it(directory), end; it != end; ++it) {
        const auto& entry = *it;
        if (is_metadata(entry.path().filename()) || entry.is_symlink()) {
            it.disable_recursion_pending();
            continue;
        }
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
    return files;
}

std::string current_branch(const fs::path& root) {
    const auto head = read_file(root / ".groot" / "HEAD");
    constexpr std::string_view prefix = "ref: refs/heads/";
    if (!head.starts_with(prefix) || head.size() <= prefix.size() + 1 || head.back() != '\n') {
        throw std::runtime_error("invalid HEAD format");
    }
    const auto branch = head.substr(prefix.size(), head.size() - prefix.size() - 1);
    if (branch == "." || branch == ".." || branch.find_first_of("/\\\r\n") != std::string::npos)
        throw std::runtime_error("invalid HEAD branch");
    return branch;
}

bool valid_hash(std::string_view hash) {
    return hash.size() == 64 && std::all_of(hash.begin(), hash.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

fs::path branch_ref(const fs::path& root) {
    return root / ".groot" / "refs" / "heads" / current_branch(root);
}

std::string branch_tip(const fs::path& root) {
    const auto value = read_file(branch_ref(root));
    if (value.empty()) return {};
    if (value.size() != 65 || value.back() != '\n' || !valid_hash(std::string_view(value).substr(0, 64))) {
        throw std::runtime_error("invalid branch reference");
    }
    return value.substr(0, 64);
}

std::string serialize_commit(const Commit& commit) {
    std::ostringstream output;
    output << "parent " << (commit.parent.empty() ? "-" : commit.parent) << '\n';
    output << "time " << commit.timestamp << '\n';
    output << "message " << std::quoted(commit.message) << '\n';
    output << "files\n";
    for (const auto& [path, hash] : commit.files) {
        output << hash << ' ' << std::quoted(path) << '\n';
    }
    return output.str();
}

Commit load_commit(const fs::path& root, const std::string& hash) {
    if (!valid_hash(hash)) throw std::runtime_error("invalid commit hash");
    const auto bytes = read_file(root / ".groot" / "commits" / hash);
    if (SHA256::hash(bytes) != hash) throw std::runtime_error("commit content does not match its SHA-256 hash");
    std::istringstream input(bytes);
    Commit result;
    result.hash = hash;
    std::string key;
    std::string parent;
    if (!(input >> key >> parent) || key != "parent" ||
        (parent != "-" && !valid_hash(parent))) throw std::runtime_error("invalid commit parent");
    result.parent = parent == "-" ? "" : parent;
    if (!(input >> key >> result.timestamp) || key != "time" || result.timestamp < 0)
        throw std::runtime_error("invalid commit time");
    if (!(input >> key >> std::quoted(result.message)) || key != "message" || result.message.empty())
        throw std::runtime_error("invalid commit message");
    std::string line;
    std::getline(input, line);
    if (!line.empty() || !std::getline(input, line) || line != "files")
        throw std::runtime_error("invalid commit header");
    Index validated(root);
    while (std::getline(input, line)) {
        std::istringstream record(line);
        std::string object_hash;
        std::string path;
        if (!(record >> object_hash >> std::quoted(path))) throw std::runtime_error("invalid commit file");
        record >> std::ws;
        if (!record.eof() || result.files.contains(path)) throw std::runtime_error("invalid commit file");
        validated.set(path, object_hash);
        result.files.emplace(path, object_hash);
    }
    if (serialize_commit(result) != bytes) throw std::runtime_error("noncanonical commit record");
    return result;
}

} // namespace

std::optional<fs::path> Repository::discover(fs::path start) {
    start = fs::absolute(start);
    if (!fs::is_directory(start)) {
        start = start.parent_path();
    }

    while (true) {
        const fs::path metadata = start / ".groot";
        if (fs::exists(metadata)) {
            if (!fs::is_directory(metadata)) {
                throw std::runtime_error(".groot exists but is not a directory: " + metadata.string());
            }
            return start;
        }
        if (start == start.root_path()) {
            return std::nullopt;
        }
        start = start.parent_path();
    }
}

fs::path Repository::init(const fs::path& root) {
    const fs::path absolute_root = fs::absolute(root);
    if (!fs::is_directory(absolute_root)) {
        throw std::runtime_error("repository directory does not exist: " + absolute_root.string());
    }
    if (discover(absolute_root)) {
        throw std::runtime_error("a Groot repository already exists here or in a parent directory");
    }

    const fs::path metadata = absolute_root / ".groot";
    fs::create_directories(metadata / "objects");
    fs::create_directories(metadata / "commits");
    fs::create_directories(metadata / "refs" / "heads");
    write_file(metadata / "HEAD", "ref: refs/heads/main\n");
    write_file(metadata / "index", "");
    write_file(metadata / "refs" / "heads" / "main", "");
    return metadata;
}

std::size_t Repository::add(const fs::path& path, const fs::path& current_directory) {
    const auto root = require_root(current_directory);
    if (path.empty()) {
        throw std::runtime_error("path does not exist");
    }
    const auto requested = fs::absolute(current_directory / path);
    if (fs::is_symlink(requested)) {
        throw std::runtime_error("symbolic links are not supported: " + requested.string());
    }
    const auto candidate = fs::weakly_canonical(requested);
    const auto scope = relative_to_root(root, candidate);

    Index index(root);
    index.load();
    ObjectStore objects(root);

    if (!fs::exists(candidate)) {
        const auto key = scope.generic_string();
        if (!index.contains(key)) {
            throw std::runtime_error("path does not exist: " + path.string());
        }
        index.remove(key); // A missing indexed file is staged for deletion.
        index.save();
        return 1;
    }

    if (fs::is_regular_file(candidate)) {
        const auto key = scope.generic_string();
        index.set(key, objects.store(read_file(candidate)));
        index.save();
        return 1;
    }
    if (!fs::is_directory(candidate)) {
        throw std::runtime_error("path is not a regular file or directory: " + path.string());
    }

    std::size_t changed = 0;
    for (const auto& file : working_files(candidate)) {
        const auto key = relative_to_root(root, file).generic_string();
        index.set(key, objects.store(read_file(file)));
        ++changed;
    }
    const auto prefix = scope == "." ? std::string{} : scope.generic_string() + "/";
    std::vector<std::string> removed;
    for (const auto& [key, hash] : index.entries()) {
        if ((prefix.empty() || key.starts_with(prefix)) &&
            (!fs::is_regular_file(root / fs::path(key)) || fs::is_symlink(root / fs::path(key)))) {
            removed.push_back(key);
        }
    }
    for (const auto& key : removed) {
        index.remove(key);
        ++changed;
    }
    index.save();
    return changed;
}

Status Repository::status(const fs::path& current_directory) {
    const auto root = require_root(current_directory);
    Index index(root);
    index.load();

    Status result;
    result.branch = current_branch(root);
    std::map<std::string, std::string> committed;
    const auto tip = branch_tip(root);
    if (!tip.empty()) committed = load_commit(root, tip).files;
    for (const auto& [key, hash] : index.entries()) {
        const auto previous = committed.find(key);
        if (previous == committed.end()) result.staged.push_back(key);
        else if (previous->second != hash) result.staged_modified.push_back(key);
        const auto file = root / fs::path(key);
        if (!fs::is_regular_file(file) || fs::is_symlink(file)) {
            result.deleted.push_back(key);
        } else if (SHA256::hash(read_file(file)) != hash) {
            result.modified.push_back(key);
        }
    }
    for (const auto& [key, hash] : committed) {
        if (!index.contains(key)) result.staged_deleted.push_back(key);
    }
    for (const auto& file : working_files(root)) {
        const auto key = relative_to_root(root, file).generic_string();
        if (!index.contains(key)) {
            result.untracked.push_back(key);
        }
    }
    std::sort(result.untracked.begin(), result.untracked.end());
    return result;
}

std::string Repository::commit(const std::string& message, const fs::path& current_directory) {
    if (message.empty() || message.find_first_of("\r\n") != std::string::npos)
        throw std::invalid_argument("commit message must be one nonempty line");
    const auto root = require_root(current_directory);
    Index index(root);
    index.load();
    const auto parent = branch_tip(root);
    if (!parent.empty() && load_commit(root, parent).files == index.entries())
        throw std::runtime_error("nothing staged to commit");
    if (parent.empty() && index.entries().empty())
        throw std::runtime_error("nothing staged to commit");
    ObjectStore objects(root);
    for (const auto& [path, hash] : index.entries()) objects.load(hash);

    Commit record;
    record.parent = parent;
    record.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    record.message = message;
    record.files = index.entries();
    const auto bytes = serialize_commit(record);
    const auto hash = SHA256::hash(bytes);
    const auto destination = root / ".groot" / "commits" / hash;
    if (fs::exists(destination)) {
        load_commit(root, hash);
    } else {
        const auto temporary = destination.string() + ".tmp." +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "." +
            std::to_string(commit_temporary_sequence++);
        try {
            write_file(temporary, bytes);
            std::error_code error;
            fs::create_hard_link(temporary, destination, error);
            if (error) {
                if (!fs::exists(destination)) throw fs::filesystem_error("cannot publish commit", temporary, destination, error);
                load_commit(root, hash);
            }
            fs::remove(temporary);
        } catch (...) {
            std::error_code ignored;
            fs::remove(temporary, ignored);
            throw;
        }
    }
    write_file(branch_ref(root), hash + "\n");
    return hash;
}

std::vector<Commit> Repository::log(const fs::path& current_directory) {
    const auto root = require_root(current_directory);
    std::vector<Commit> history;
    std::set<std::string> seen;
    auto hash = branch_tip(root);
    while (!hash.empty()) {
        if (!seen.insert(hash).second) throw std::runtime_error("cycle in commit history");
        history.push_back(load_commit(root, hash));
        hash = history.back().parent;
    }
    return history;
}

} // namespace groot
