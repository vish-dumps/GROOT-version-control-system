#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace groot {

// The index is the next complete snapshot, keyed by repository-relative paths.
class Index {
public:
    explicit Index(std::filesystem::path repository_root);

    void load();
    void save() const;
    void set(const std::string& path, const std::string& hash);
    void remove(const std::string& path);
    bool contains(const std::string& path) const;
    const std::map<std::string, std::string>& entries() const noexcept { return entries_; }

private:
    std::filesystem::path index_path_;
    std::map<std::string, std::string> entries_;
    static void validate_path(const std::string& path);
    static void validate_hash(const std::string& hash);
};

} // namespace groot
