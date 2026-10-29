#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace groot {

struct Status {
    std::string branch;
    std::vector<std::string> staged;
    std::vector<std::string> modified;
    std::vector<std::string> deleted;
    std::vector<std::string> untracked;
};

class Repository {
public:
    // Searches from start toward the filesystem root.
    static std::optional<std::filesystem::path> discover(
        std::filesystem::path start = std::filesystem::current_path());

    // Creates metadata in root. Refuses to initialize inside another repository.
    static std::filesystem::path init(const std::filesystem::path& root);

    // Returns the number of files added or removed from the index.
    static std::size_t add(const std::filesystem::path& path,
                           const std::filesystem::path& current_directory = std::filesystem::current_path());
    static Status status(const std::filesystem::path& current_directory = std::filesystem::current_path());
};

} // namespace groot
