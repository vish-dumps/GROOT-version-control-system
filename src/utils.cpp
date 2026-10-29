#include "groot/utils.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace groot {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file for reading: " + path.string());
    }
    std::string bytes(std::istreambuf_iterator<char>{input}, {});
    if (input.bad()) {
        throw std::runtime_error("cannot read file: " + path.string());
    }
    return bytes;
}

void write_file(const std::filesystem::path& path, std::string_view bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("cannot open file for writing: " + path.string());
    }
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    if (!output) {
        throw std::runtime_error("cannot write file: " + path.string());
    }
}

} // namespace groot
