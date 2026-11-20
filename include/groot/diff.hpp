#pragma once

#include <string>
#include <string_view>
#include <optional>

namespace groot {

// Returns a unified, whole-file line diff. Empty output means identical content.
std::string line_diff(std::string_view before, std::string_view after,
                      const std::string& path);
// Returns merged text when line edits do not overlap; nullopt signals a conflict.
std::optional<std::string> merge_lines(std::string_view base, std::string_view ours,
                                       std::string_view theirs);

} // namespace groot
