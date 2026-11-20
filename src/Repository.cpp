#include "groot/repository.hpp"

#include "groot/index.hpp"
#include "groot/diff.hpp"
#include "groot/object_store.hpp"
#include "groot/sha256.hpp"
#include "groot/utils.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <deque>
#include <iomanip>
#include <limits>
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

bool has_symlink_component(const fs::path& root, const fs::path& relative) {
    auto current = root;
    for (const auto& component : relative) {
        current /= component;
        if (fs::is_symlink(current)) return true;
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

std::optional<std::string> detached_head(const fs::path& root) {
    const auto head = read_file(root / ".groot" / "HEAD");
    constexpr std::string_view prefix = "commit: ";
    if (!head.starts_with(prefix)) return std::nullopt;
    if (head.size() != prefix.size() + 65 || head.back() != '\n' ||
        !valid_hash(std::string_view(head).substr(prefix.size(), 64)))
        throw std::runtime_error("invalid detached HEAD");
    return head.substr(prefix.size(), 64);
}

void validate_branch_name(const std::string& name) {
    if (name.empty() || name == "." || name == ".." || name.front() == '-' || name.front() == '.' ||
        name.back() == '.' ||
        !std::all_of(name.begin(), name.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
        })) throw std::invalid_argument("invalid branch name");
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    const auto device = upper.substr(0, upper.find('.'));
    if (device == "CON" || device == "PRN" || device == "AUX" || device == "NUL" ||
        (device.size() == 4 && (device.starts_with("COM") || device.starts_with("LPT")) &&
         device[3] >= '1' && device[3] <= '9'))
        throw std::invalid_argument("reserved branch name");
}

fs::path named_branch_ref(const fs::path& root, const std::string& name) {
    validate_branch_name(name);
    return root / ".groot" / "refs" / "heads" / name;
}

fs::path branch_ref(const fs::path& root) {
    return named_branch_ref(root, current_branch(root));
}

std::string branch_tip(const fs::path& root, const std::string& name) {
    const auto ref = named_branch_ref(root, name);
    if (fs::is_symlink(ref)) throw std::runtime_error("symbolic branch reference is not supported");
    const auto value = read_file(ref);
    if (value.empty()) return {};
    if (value.size() != 65 || value.back() != '\n' || !valid_hash(std::string_view(value).substr(0, 64))) {
        throw std::runtime_error("invalid branch reference");
    }
    return value.substr(0, 64);
}

std::string branch_tip(const fs::path& root) { return branch_tip(root, current_branch(root)); }

std::string head_tip(const fs::path& root) {
    if (const auto detached = detached_head(root)) return *detached;
    return branch_tip(root);
}

std::string serialize_commit(const Commit& commit) {
    std::ostringstream output;
    output << "parent " << (commit.parent.empty() ? "-" : commit.parent) << '\n';
    if (!commit.merge_parent.empty()) output << "merge-parent " << commit.merge_parent << '\n';
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
    if (!(input >> key)) throw std::runtime_error("invalid commit header");
    if (key == "merge-parent") {
        if (!(input >> result.merge_parent) || !valid_hash(result.merge_parent))
            throw std::runtime_error("invalid merge parent");
        if (!(input >> key)) throw std::runtime_error("invalid commit header");
    }
    if (key != "time" || !(input >> result.timestamp) || result.timestamp < 0)
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

std::string save_commit(const fs::path& root, const Commit& record) {
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
    return hash;
}

std::int64_t now_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

using Snapshot = std::map<std::string, std::string>;

Snapshot snapshot_at(const fs::path& root, const std::string& hash) {
    return hash.empty() ? Snapshot{} : load_commit(root, hash).files;
}

void require_clean(const fs::path& root) {
    if (fs::exists(root / ".groot" / "MERGE_HEAD"))
        throw std::runtime_error("finish the current merge before switching branches or merging");
    const auto state = Repository::status(root);
    if (!state.staged.empty() || !state.staged_modified.empty() || !state.staged_deleted.empty() ||
        !state.modified.empty() || !state.deleted.empty())
        throw std::runtime_error("commit or discard tracked changes before switching branches or merging");
}

void validate_snapshot_paths(const Snapshot& files) {
    for (const auto& [path, hash] : files) {
        auto slash = path.find('/');
        while (slash != std::string::npos) {
            if (files.contains(path.substr(0, slash)))
                throw std::runtime_error("snapshot contains conflicting file paths");
            slash = path.find('/', slash + 1);
        }
    }
}

void apply_snapshot(const fs::path& root, const Snapshot& before, const Snapshot& after) {
    validate_snapshot_paths(after);
    ObjectStore objects(root);
    std::map<std::string, std::string> content;
    for (const auto& [path, hash] : after) {
        if (!before.contains(path) || before.at(path) != hash) content.emplace(path, objects.load(hash));
    }
    for (const auto& [path, hash] : after) {
        const auto destination = root / fs::path(path);
        if (fs::is_symlink(destination))
            throw std::runtime_error("symbolic link blocks checkout: " + destination.string());
        auto parent = destination.parent_path();
        while (parent != root && !parent.empty()) {
            if (fs::is_symlink(parent)) throw std::runtime_error("symbolic link blocks checkout: " + parent.string());
            if (fs::exists(parent) && !fs::is_directory(parent) && !before.contains(parent.lexically_relative(root).generic_string()))
                throw std::runtime_error("untracked file blocks checkout: " + parent.string());
            parent = parent.parent_path();
        }
        if (!before.contains(path) && fs::exists(destination)) {
            if (!fs::is_directory(destination))
                throw std::runtime_error("untracked file blocks checkout: " + destination.string());
            for (fs::recursive_directory_iterator it(destination), end; it != end; ++it) {
                if (it->is_symlink()) it.disable_recursion_pending();
                const auto file = it->path();
                const auto key = file.lexically_relative(root).generic_string();
                if (it->is_directory() && !it->is_symlink()) {
                    bool contains_tracked = false;
                    for (const auto& [tracked, tracked_hash] : before) {
                        if (tracked.starts_with(key + "/")) { contains_tracked = true; break; }
                    }
                    if (!contains_tracked)
                        throw std::runtime_error("untracked directory blocks checkout: " + file.string());
                } else if (!before.contains(key)) {
                    throw std::runtime_error("untracked file blocks checkout: " + file.string());
                }
            }
            bool contains_tracked = false;
            for (const auto& [tracked, tracked_hash] : before) {
                if (tracked.starts_with(path + "/")) { contains_tracked = true; break; }
            }
            if (!contains_tracked)
                throw std::runtime_error("untracked directory blocks checkout: " + destination.string());
        }
    }
    for (const auto& [path, hash] : before) {
        if (!after.contains(path)) {
            const auto removed = root / fs::path(path);
            fs::remove(removed);
            for (auto parent = removed.parent_path(); parent != root && !parent.empty(); parent = parent.parent_path()) {
                std::error_code error;
                if (!fs::remove(parent, error)) break; // Keep nonempty directories.
            }
        }
    }
    for (const auto& [path, bytes] : content) {
        const auto destination = root / fs::path(path);
        if (fs::is_directory(destination)) fs::remove(destination);
        fs::create_directories(destination.parent_path());
        write_file(destination, bytes);
    }
    Index index(root);
    index.load();
    for (const auto& [path, hash] : before) index.remove(path);
    for (const auto& [path, hash] : after) index.set(path, hash);
    index.save();
}

std::map<std::string, int> ancestor_distances(const fs::path& root, const std::string& tip) {
    std::map<std::string, int> distances;
    std::deque<std::pair<std::string, int>> pending;
    if (!tip.empty()) pending.emplace_back(tip, 0);
    while (!pending.empty()) {
        const auto [hash, distance] = pending.front();
        pending.pop_front();
        if (distances.contains(hash) && distances.at(hash) <= distance) continue;
        distances[hash] = distance;
        const auto record = load_commit(root, hash);
        if (!record.parent.empty()) pending.emplace_back(record.parent, distance + 1);
        if (!record.merge_parent.empty()) pending.emplace_back(record.merge_parent, distance + 1);
    }
    return distances;
}

std::string merge_base(const fs::path& root, const std::string& ours, const std::string& theirs) {
    const auto left = ancestor_distances(root, ours);
    const auto right = ancestor_distances(root, theirs);
    // Reachability decides ancestry; shortest-path scores can prefer an older
    // ancestor when a merge parent creates a shortcut through the graph.
    if (left.contains(theirs)) return theirs;
    if (right.contains(ours)) return ours;
    std::string best;
    auto best_score = std::numeric_limits<int>::max();
    for (const auto& [hash, distance] : left) {
        const auto other = right.find(hash);
        if (other != right.end() && distance + other->second < best_score) {
            best = hash;
            best_score = distance + other->second;
        }
    }
    return best;
}

std::string file_hash(const Snapshot& files, const std::string& path) {
    const auto it = files.find(path);
    return it == files.end() ? std::string{} : it->second;
}

std::string conflict_content(const std::string& ours, const std::string& theirs,
                             const std::string& other_branch) {
    return "<<<<<<< HEAD\n" + ours + (ours.empty() || ours.back() == '\n' ? "" : "\n") +
           "=======\n" + theirs + (theirs.empty() || theirs.back() == '\n' ? "" : "\n") +
           ">>>>>>> " + other_branch + "\n";
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
        std::vector<std::string> incompatible;
        for (const auto& [indexed, hash] : index.entries()) {
            if (indexed.starts_with(key + "/") || key.starts_with(indexed + "/"))
                incompatible.push_back(indexed);
        }
        for (const auto& indexed : incompatible) index.remove(indexed);
        index.save();
        return 1 + incompatible.size();
    }
    if (!fs::is_directory(candidate)) {
        throw std::runtime_error("path is not a regular file or directory: " + path.string());
    }

    std::size_t changed = 0;
    const auto scope_key = scope.generic_string();
    std::vector<std::string> old_files;
    for (const auto& [key, hash] : index.entries()) {
        if (key == scope_key || scope_key.starts_with(key + "/")) old_files.push_back(key);
    }
    for (const auto& key : old_files) {
        index.remove(key);
        ++changed;
    }
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
    if (const auto detached = detached_head(root)) {
        result.detached = true;
        result.branch = detached->substr(0, 12);
    } else {
        result.branch = current_branch(root);
    }
    result.merging = fs::exists(root / ".groot" / "MERGE_HEAD");
    std::map<std::string, std::string> committed;
    const auto tip = head_tip(root);
    if (!tip.empty()) committed = load_commit(root, tip).files;
    for (const auto& [key, hash] : index.entries()) {
        const auto previous = committed.find(key);
        if (previous == committed.end()) result.staged.push_back(key);
        else if (previous->second != hash) result.staged_modified.push_back(key);
        const auto file = root / fs::path(key);
        if (has_symlink_component(root, fs::path(key)) || !fs::is_regular_file(file)) {
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
    if (detached_head(root)) throw std::runtime_error("create and check out a branch before committing from a historical commit");
    Index index(root);
    index.load();
    const auto parent = branch_tip(root);
    const auto merge_head_path = root / ".groot" / "MERGE_HEAD";
    std::string merge_parent;
    if (fs::exists(merge_head_path)) {
        const auto line = read_file(merge_head_path);
        if (line.size() != 65 || line.back() != '\n' || !valid_hash(std::string_view(line).substr(0, 64)))
            throw std::runtime_error("invalid MERGE_HEAD");
        merge_parent = line.substr(0, 64);
        load_commit(root, merge_parent);
        std::istringstream conflicts(read_file(root / ".groot" / "MERGE_CONFLICTS"));
        std::string path;
        while (std::getline(conflicts, path)) {
            const auto file = root / fs::path(path);
            const auto staged = index.entries().find(path);
            if (!has_symlink_component(root, fs::path(path)) && fs::is_regular_file(file)) {
                if (staged == index.entries().end() || SHA256::hash(read_file(file)) != staged->second)
                    throw std::runtime_error("resolve and stage merge conflicts before committing: " + path);
            } else if (staged != index.entries().end()) {
                throw std::runtime_error("resolve and stage merge conflicts before committing: " + path);
            }
        }
    }
    if (merge_parent.empty() && !parent.empty() && load_commit(root, parent).files == index.entries())
        throw std::runtime_error("nothing staged to commit");
    if (merge_parent.empty() && parent.empty() && index.entries().empty())
        throw std::runtime_error("nothing staged to commit");
    ObjectStore objects(root);
    validate_snapshot_paths(index.entries());
    for (const auto& [path, hash] : index.entries()) objects.load(hash);

    Commit record;
    record.parent = parent;
    record.merge_parent = merge_parent;
    record.timestamp = now_seconds();
    record.message = message;
    record.files = index.entries();
    const auto hash = save_commit(root, record);
    write_file(branch_ref(root), hash + "\n");
    if (!merge_parent.empty()) {
        fs::remove(merge_head_path);
        fs::remove(root / ".groot" / "MERGE_CONFLICTS");
    }
    return hash;
}

std::vector<Commit> Repository::log(const fs::path& current_directory) {
    const auto root = require_root(current_directory);
    std::vector<Commit> history;
    std::set<std::string> seen;
    auto hash = head_tip(root);
    while (!hash.empty()) {
        if (!seen.insert(hash).second) throw std::runtime_error("cycle in commit history");
        history.push_back(load_commit(root, hash));
        hash = history.back().parent;
    }
    return history;
}

std::string Repository::diff(bool staged, const fs::path& current_directory) {
    const auto root = require_root(current_directory);
    Index index(root);
    index.load();
    const auto committed = snapshot_at(root, head_tip(root));
    std::set<std::string> paths;
    for (const auto& [path, hash] : index.entries()) paths.insert(path);
    if (staged) for (const auto& [path, hash] : committed) paths.insert(path);
    ObjectStore objects(root);
    std::string output;
    for (const auto& path : paths) {
        std::string before;
        std::string after;
        if (staged) {
            const auto old_hash = file_hash(committed, path);
            const auto new_hash = file_hash(index.entries(), path);
            if (old_hash == new_hash) continue;
            if (!old_hash.empty()) before = objects.load(old_hash);
            if (!new_hash.empty()) after = objects.load(new_hash);
        } else {
            const auto it = index.entries().find(path);
            if (it == index.entries().end()) continue;
            before = objects.load(it->second);
            const auto file = root / fs::path(path);
            if (!has_symlink_component(root, fs::path(path)) && fs::is_regular_file(file)) after = read_file(file);
        }
        auto change = line_diff(before, after, path);
        // A missing path and an empty file have equal bytes, but different
        // snapshot identities. Preserve that structural change in the diff.
        if (change.empty()) {
            const bool existed = staged ? committed.contains(path) : index.contains(path);
            const auto file = root / fs::path(path);
            const bool exists = staged ? index.contains(path)
                : !has_symlink_component(root, fs::path(path)) && fs::is_regular_file(file);
            if (existed != exists) {
                change = "diff --groot a/" + path + " b/" + path + "\n" +
                    (exists ? "new empty file\n--- /dev/null\n+++ b/" + path + "\n"
                            : "deleted empty file\n--- a/" + path + "\n+++ /dev/null\n");
            }
        }
        output += change;
    }
    return output;
}

std::vector<std::string> Repository::branches(const fs::path& current_directory) {
    const auto root = require_root(current_directory);
    std::vector<std::string> names;
    for (const auto& entry : fs::directory_iterator(root / ".groot" / "refs" / "heads")) {
        if (!entry.is_regular_file() || entry.is_symlink()) continue;
        const auto name = entry.path().filename().string();
        validate_branch_name(name);
        branch_tip(root, name);
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

void Repository::create_branch(const std::string& name, const fs::path& current_directory) {
    const auto root = require_root(current_directory);
    const auto destination = named_branch_ref(root, name);
    if (fs::exists(destination)) throw std::runtime_error("branch already exists: " + name);
    const auto tip = head_tip(root);
    write_file(destination, tip.empty() ? "" : tip + "\n");
}

void Repository::checkout(const std::string& name, const fs::path& current_directory) {
    const auto root = require_root(current_directory);
    const auto detached = detached_head(root);
    const auto return_path = root / ".groot" / "RETURN_BRANCH";
    std::string target_branch;
    std::string target_hash;
    if (name == "--latest") {
        if (!detached) throw std::runtime_error("HEAD is already on a branch");
        const auto saved = read_file(return_path);
        if (saved.empty() || saved.back() != '\n') throw std::runtime_error("invalid return branch");
        target_branch = saved.substr(0, saved.size() - 1);
        validate_branch_name(target_branch);
    } else if (valid_hash(name) && !fs::exists(named_branch_ref(root, name)) &&
               !fs::is_symlink(named_branch_ref(root, name))) {
        load_commit(root, name);
        target_hash = name;
    } else {
        target_branch = name;
        validate_branch_name(target_branch);
    }
    if (!target_branch.empty()) {
        const auto ref = named_branch_ref(root, target_branch);
        if (!fs::is_regular_file(ref) || fs::is_symlink(ref))
            throw std::runtime_error("branch does not exist: " + target_branch);
        if (!detached && target_branch == current_branch(root)) return;
        target_hash = branch_tip(root, target_branch);
    } else if (detached && *detached == target_hash) {
        return;
    }
    require_clean(root);
    const auto before = snapshot_at(root, head_tip(root));
    const auto after = snapshot_at(root, target_hash);
    apply_snapshot(root, before, after);
    if (!target_branch.empty()) {
        write_file(root / ".groot" / "HEAD", "ref: refs/heads/" + target_branch + "\n");
        if (fs::exists(return_path)) fs::remove(return_path);
    } else {
        if (!detached) write_file(return_path, current_branch(root) + "\n");
        write_file(root / ".groot" / "HEAD", "commit: " + target_hash + "\n");
    }
}

MergeResult Repository::merge(const std::string& name, const fs::path& current_directory) {
    const auto root = require_root(current_directory);
    if (detached_head(root)) throw std::runtime_error("check out a branch before merging");
    validate_branch_name(name);
    if (!fs::is_regular_file(named_branch_ref(root, name)) || fs::is_symlink(named_branch_ref(root, name)))
        throw std::runtime_error("branch does not exist: " + name);
    if (name == current_branch(root)) throw std::runtime_error("cannot merge a branch into itself");
    require_clean(root);
    const auto ours = branch_tip(root);
    const auto theirs = branch_tip(root, name);
    if (theirs.empty()) return {MergeResult::Kind::AlreadyUpToDate, ours, {}};
    const auto base = merge_base(root, ours, theirs);
    if (base == theirs) return {MergeResult::Kind::AlreadyUpToDate, ours, {}};
    const auto before = snapshot_at(root, ours);
    const auto other = snapshot_at(root, theirs);
    if (base == ours) {
        apply_snapshot(root, before, other);
        write_file(branch_ref(root), theirs + "\n");
        return {MergeResult::Kind::FastForward, theirs, {}};
    }

    const auto ancestor = snapshot_at(root, base);
    Snapshot merged = before;
    std::set<std::string> paths;
    for (const auto& [path, hash] : ancestor) paths.insert(path);
    for (const auto& [path, hash] : before) paths.insert(path);
    for (const auto& [path, hash] : other) paths.insert(path);
    std::vector<std::string> conflicts;
    std::map<std::string, std::string> markers;
    ObjectStore objects(root);
    for (const auto& path : paths) {
        const auto old_hash = file_hash(ancestor, path);
        const auto our_hash = file_hash(before, path);
        const auto their_hash = file_hash(other, path);
        if (our_hash == their_hash || their_hash == old_hash) continue;
        if (our_hash == old_hash) {
            if (their_hash.empty()) merged.erase(path);
            else merged[path] = their_hash;
            continue;
        }
        const auto our_bytes = our_hash.empty() ? std::string{} : objects.load(our_hash);
        const auto their_bytes = their_hash.empty() ? std::string{} : objects.load(their_hash);
        if (!old_hash.empty() && !our_hash.empty() && !their_hash.empty()) {
            const auto base_bytes = objects.load(old_hash);
            if (const auto combined = merge_lines(base_bytes, our_bytes, their_bytes)) {
                merged[path] = objects.store(*combined);
                continue;
            }
        }
        if (our_bytes.find('\0') != std::string::npos || their_bytes.find('\0') != std::string::npos)
            throw std::runtime_error("binary merge conflict requires manual handling: " + path);
        conflicts.push_back(path);
        markers[path] = conflict_content(our_bytes, their_bytes, name);
    }
    validate_snapshot_paths(merged);
    for (const auto& [path, bytes] : markers) {
        const auto destination = root / fs::path(path);
        if (!before.contains(path) && (fs::exists(destination) || fs::is_symlink(destination)))
            throw std::runtime_error("untracked file blocks merge conflict marker: " + destination.string());
        for (auto parent = destination.parent_path(); parent != root && !parent.empty(); parent = parent.parent_path()) {
            if (fs::is_symlink(parent) || (fs::exists(parent) && !fs::is_directory(parent)))
                throw std::runtime_error("path blocks merge conflict marker: " + parent.string());
        }
    }
    apply_snapshot(root, before, merged);
    write_file(root / ".groot" / "MERGE_HEAD", theirs + "\n");
    std::string conflict_list;
    for (const auto& path : conflicts) conflict_list += path + "\n";
    write_file(root / ".groot" / "MERGE_CONFLICTS", conflict_list);
    for (const auto& [path, bytes] : markers) {
        const auto destination = root / fs::path(path);
        fs::create_directories(destination.parent_path());
        write_file(destination, bytes);
    }
    if (!conflicts.empty()) return {MergeResult::Kind::Conflicts, {}, conflicts};
    return {MergeResult::Kind::Merged, Repository::commit("Merge branch '" + name + "'", root), {}};
}

void Repository::abort_merge(const fs::path& current_directory) {
    const auto root = require_root(current_directory);
    const auto merge_head = root / ".groot" / "MERGE_HEAD";
    if (!fs::exists(merge_head)) throw std::runtime_error("no merge in progress");
    const auto original = snapshot_at(root, branch_tip(root));
    Index index(root);
    index.load();
    std::istringstream paths(read_file(root / ".groot" / "MERGE_CONFLICTS"));
    std::string path;
    while (std::getline(paths, path)) {
        const auto file = root / fs::path(path);
        relative_to_root(root, file);
        if (fs::exists(file) || fs::is_symlink(file)) {
            if (fs::is_directory(file) && !fs::is_symlink(file))
                throw std::runtime_error("directory blocks merge abort: " + file.string());
            fs::remove(file);
        }
    }
    apply_snapshot(root, index.entries(), original);
    ObjectStore objects(root);
    for (const auto& [name, hash] : original) {
        const auto file = root / fs::path(name);
        fs::create_directories(file.parent_path());
        write_file(file, objects.load(hash));
    }
    fs::remove(merge_head);
    fs::remove(root / ".groot" / "MERGE_CONFLICTS");
}

} // namespace groot
