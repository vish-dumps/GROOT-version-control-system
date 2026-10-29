#include "groot/index.hpp"

#include "groot/utils.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;

namespace groot {

Index::Index(fs::path repository_root)
    : index_path_(std::move(repository_root) / ".groot" / "index") {}

void Index::validate_path(const std::string& path) {
    const fs::path parsed(path);
    if (path.empty() || path == "." || path.find_first_of("\r\n") != std::string::npos ||
        parsed.has_root_name() || parsed.has_root_directory() ||
        parsed != parsed.lexically_normal() || parsed.generic_string() != path) {
        throw std::invalid_argument("invalid index path");
    }
    for (const auto& component : parsed) {
        if (component == ".." || component == ".groot" || component == ".git") {
            throw std::invalid_argument("index path points outside versioned files");
        }
    }
}

void Index::validate_hash(const std::string& hash) {
    if (hash.size() != 64 || !std::all_of(hash.begin(), hash.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        })) {
        throw std::invalid_argument("invalid SHA-256 hash in index");
    }
}

void Index::load() {
    entries_.clear();
    std::istringstream input(read_file(index_path_));
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream record(line);
        std::string hash;
        std::string path;
        if (!(record >> hash >> std::quoted(path))) {
            throw std::runtime_error("invalid index record");
        }
        record >> std::ws;
        if (!record.eof()) {
            throw std::runtime_error("unexpected data in index record");
        }
        validate_hash(hash);
        validate_path(path);
        if (!entries_.emplace(path, hash).second) {
            throw std::runtime_error("duplicate path in index: " + path);
        }
    }
}

void Index::save() const {
    std::ostringstream output;
    for (const auto& [path, hash] : entries_) {
        output << hash << ' ' << std::quoted(path) << '\n';
    }
    write_file(index_path_, output.str());
}

void Index::set(const std::string& path, const std::string& hash) {
    validate_path(path);
    validate_hash(hash);
    entries_[path] = hash;
}

void Index::remove(const std::string& path) {
    entries_.erase(path);
}

bool Index::contains(const std::string& path) const {
    return entries_.contains(path);
}

} // namespace groot
