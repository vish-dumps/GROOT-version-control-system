#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace groot {

std::string read_file(const std::filesystem::path& path);
void write_file(const std::filesystem::path& path, std::string_view bytes);

} // namespace groot
