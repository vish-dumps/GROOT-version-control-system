#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace groot {

class ObjectStore {
public:
    explicit ObjectStore(std::filesystem::path repository_root);

    std::string store(std::string_view content) const;
    std::string load(std::string_view hash) const;
    bool exists(std::string_view hash) const;

private:
    std::filesystem::path objects_path_;
    static void validate_hash(std::string_view hash);
    std::filesystem::path path_for(std::string_view hash) const;
};

} // namespace groot
