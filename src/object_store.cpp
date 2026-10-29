#include "groot/object_store.hpp"

#include "groot/sha256.hpp"
#include "groot/utils.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <stdexcept>

namespace fs = std::filesystem;

namespace groot {
namespace {
std::atomic<std::uint64_t> temporary_sequence{0};
}

ObjectStore::ObjectStore(fs::path repository_root)
    : objects_path_(std::move(repository_root) / ".groot" / "objects") {
    if (!fs::is_directory(objects_path_)) {
        throw std::runtime_error("object store does not exist: " + objects_path_.string());
    }
}

void ObjectStore::validate_hash(std::string_view hash) {
    const bool valid = hash.size() == 64 &&
        std::all_of(hash.begin(), hash.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        });
    if (!valid) {
        throw std::invalid_argument("invalid SHA-256 object hash");
    }
}

fs::path ObjectStore::path_for(std::string_view hash) const {
    validate_hash(hash);
    return objects_path_ / std::string(hash);
}

bool ObjectStore::exists(std::string_view hash) const {
    return fs::is_regular_file(path_for(hash));
}

std::string ObjectStore::load(std::string_view hash) const {
    const auto content = read_file(path_for(hash));
    if (SHA256::hash(content) != hash) {
        throw std::runtime_error("object content does not match its SHA-256 hash");
    }
    return content;
}

std::string ObjectStore::store(std::string_view content) const {
    const auto hash = SHA256::hash(content);
    const auto destination = path_for(hash);
    if (fs::exists(destination)) {
        load(hash); // Detect corruption instead of silently trusting an existing name.
        return hash;
    }

    // A temporary file and a hard link make publication atomic without replacing an object.
    const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
                      + "." + std::to_string(temporary_sequence++);
    const auto temporary = objects_path_ / (hash + ".tmp." + suffix);
    try {
        write_file(temporary, content);
        std::error_code error;
        fs::create_hard_link(temporary, destination, error);
        if (error) {
            if (!fs::exists(destination)) {
                throw fs::filesystem_error("cannot publish object", temporary, destination, error);
            }
            load(hash); // Another writer published it; verify identical content.
        }
        fs::remove(temporary);
    } catch (...) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        throw;
    }
    return hash;
}

} // namespace groot
