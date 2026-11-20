#include "groot/diff.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace groot {
namespace {

std::vector<std::string> lines(std::string_view bytes) {
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start < bytes.size()) {
        const auto end = bytes.find('\n', start);
        if (end == std::string_view::npos) {
            result.emplace_back(bytes.substr(start));
            break;
        }
        result.emplace_back(bytes.substr(start, end - start + 1));
        start = end + 1;
    }
    return result;
}

struct Edit {
    std::size_t begin;
    std::size_t end;
    std::vector<std::string> replacement;
};

std::vector<Edit> edits(const std::vector<std::string>& base,
                        const std::vector<std::string>& changed) {
    if ((base.size() + 1) > 10'000'000 / (changed.size() + 1))
        throw std::runtime_error("file is too large for the LCS merge algorithm");
    const auto width = changed.size() + 1;
    std::vector<std::size_t> lcs((base.size() + 1) * width);
    for (std::size_t i = base.size(); i-- > 0;) {
        for (std::size_t j = changed.size(); j-- > 0;) {
            lcs[i * width + j] = base[i] == changed[j]
                ? 1 + lcs[(i + 1) * width + j + 1]
                : std::max(lcs[(i + 1) * width + j], lcs[i * width + j + 1]);
        }
    }
    std::vector<Edit> result;
    std::optional<Edit> current;
    std::size_t i = 0, j = 0;
    while (i < base.size() || j < changed.size()) {
        if (i < base.size() && j < changed.size() && base[i] == changed[j]) {
            if (current) { result.push_back(std::move(*current)); current.reset(); }
            ++i; ++j;
        } else if (j < changed.size() && (i == base.size() ||
                   lcs[i * width + j + 1] >= lcs[(i + 1) * width + j])) {
            if (!current) current = Edit{i, i, {}};
            current->replacement.push_back(changed[j++]);
        } else {
            if (!current) current = Edit{i, i, {}};
            ++current->end;
            ++i;
        }
    }
    if (current) result.push_back(std::move(*current));
    return result;
}

} // namespace

std::string line_diff(std::string_view before, std::string_view after,
                      const std::string& path) {
    if (before == after) return {};
    std::ostringstream output;
    output << "diff --groot a/" << path << " b/" << path << '\n';
    if (before.find('\0') != std::string_view::npos || after.find('\0') != std::string_view::npos) {
        output << "Binary files differ\n";
        return output.str();
    }
    const auto left = lines(before);
    const auto right = lines(after);
    if ((left.size() + 1) > 10'000'000 / (right.size() + 1))
        throw std::runtime_error("diff is too large for the LCS algorithm: " + path);
    const auto width = right.size() + 1;
    std::vector<std::size_t> lcs((left.size() + 1) * width);
    for (std::size_t i = left.size(); i-- > 0;) {
        for (std::size_t j = right.size(); j-- > 0;) {
            lcs[i * width + j] = left[i] == right[j]
                ? 1 + lcs[(i + 1) * width + j + 1]
                : std::max(lcs[(i + 1) * width + j], lcs[i * width + j + 1]);
        }
    }
    output << "--- a/" << path << "\n+++ b/" << path << '\n';
    output << "@@ -" << (left.empty() ? 0 : 1) << ',' << left.size()
           << " +" << (right.empty() ? 0 : 1) << ',' << right.size() << " @@\n";
    std::size_t i = 0, j = 0;
    while (i < left.size() || j < right.size()) {
        const std::string* line;
        char prefix;
        if (i < left.size() && j < right.size() && left[i] == right[j]) {
            prefix = ' ';
            line = &left[i++];
            ++j;
        } else if (j < right.size() && (i == left.size() ||
                   lcs[i * width + j + 1] >= lcs[(i + 1) * width + j])) {
            prefix = '+';
            line = &right[j++];
        } else {
            prefix = '-';
            line = &left[i++];
        }
        output << prefix << *line;
        if (line->empty() || line->back() != '\n') output << "\n\\ No newline at end of file\n";
    }
    return output.str();
}

std::optional<std::string> merge_lines(std::string_view base, std::string_view ours,
                                       std::string_view theirs) {
    if (base.find('\0') != std::string_view::npos || ours.find('\0') != std::string_view::npos ||
        theirs.find('\0') != std::string_view::npos) return std::nullopt;
    const auto original = lines(base);
    const auto left = edits(original, lines(ours));
    const auto right = edits(original, lines(theirs));
    std::vector<Edit> combined = left;
    for (const auto& candidate : right) {
        bool duplicate = false;
        for (const auto& existing : left) {
            const bool same = existing.begin == candidate.begin && existing.end == candidate.end &&
                              existing.replacement == candidate.replacement;
            if (same) { duplicate = true; break; }
            const bool overlap = existing.begin < candidate.end && candidate.begin < existing.end;
            const bool same_insertion = existing.begin == existing.end &&
                                        candidate.begin == candidate.end && existing.begin == candidate.begin;
            const bool insertion_inside =
                (existing.begin == existing.end && existing.begin > candidate.begin && existing.begin < candidate.end) ||
                (candidate.begin == candidate.end && candidate.begin > existing.begin && candidate.begin < existing.end);
            if (overlap || same_insertion || insertion_inside) return std::nullopt;
        }
        if (!duplicate) combined.push_back(candidate);
    }
    std::sort(combined.begin(), combined.end(), [](const Edit& a, const Edit& b) {
        return a.begin < b.begin || (a.begin == b.begin && a.end < b.end);
    });
    std::string result;
    std::size_t cursor = 0;
    for (const auto& change : combined) {
        for (; cursor < change.begin; ++cursor) result += original[cursor];
        for (const auto& line : change.replacement) result += line;
        cursor = change.end;
    }
    for (; cursor < original.size(); ++cursor) result += original[cursor];
    return result;
}

} // namespace groot
