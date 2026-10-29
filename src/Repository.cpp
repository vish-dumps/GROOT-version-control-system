#include "groot/repository.hpp"

#include "groot/index.hpp"
#include "groot/object_store.hpp"
#include "groot/sha256.hpp"
#include "groot/utils.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace fs = std::filesystem;

namespace groot {
namespace {

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
    return head.substr(prefix.size(), head.size() - prefix.size() - 1);
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
    for (const auto& [key, hash] : index.entries()) {
        result.staged.push_back(key);
        const auto file = root / fs::path(key);
        if (!fs::is_regular_file(file) || fs::is_symlink(file)) {
            result.deleted.push_back(key);
        } else if (SHA256::hash(read_file(file)) != hash) {
            result.modified.push_back(key);
        }
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

} // namespace groot
