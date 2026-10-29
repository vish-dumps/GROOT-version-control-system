#pragma once

#include <string>
#include <string_view>

namespace groot {

class SHA256 {
public:
    // Returns a 64-character lowercase hex digest of arbitrary bytes.
    static std::string hash(std::string_view bytes);
};

} // namespace groot
