#include "groot/sha256.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <vector>

namespace groot {
namespace {

constexpr std::array<std::uint32_t, 64> round_constants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

} // namespace

std::string SHA256::hash(std::string_view bytes) {
    std::vector<std::uint8_t> padded(bytes.begin(), bytes.end());
    const auto bit_length = static_cast<std::uint64_t>(bytes.size()) * 8;
    padded.push_back(0x80);
    while (padded.size() % 64 != 56) {
        padded.push_back(0);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        padded.push_back(static_cast<std::uint8_t>(bit_length >> shift));
    }

    std::array<std::uint32_t, 8> digest = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    for (std::size_t offset = 0; offset < padded.size(); offset += 64) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t i = 0; i < 16; ++i) {
            const auto p = offset + i * 4;
            words[i] = (std::uint32_t(padded[p]) << 24)
                     | (std::uint32_t(padded[p + 1]) << 16)
                     | (std::uint32_t(padded[p + 2]) << 8)
                     | std::uint32_t(padded[p + 3]);
        }
        for (std::size_t i = 16; i < 64; ++i) {
            const auto x = words[i - 15];
            const auto y = words[i - 2];
            const auto small0 = std::rotr(x, 7) ^ std::rotr(x, 18) ^ (x >> 3);
            const auto small1 = std::rotr(y, 17) ^ std::rotr(y, 19) ^ (y >> 10);
            words[i] = words[i - 16] + small0 + words[i - 7] + small1;
        }

        auto [a, b, c, d, e, f, g, h] = digest;
        for (std::size_t i = 0; i < 64; ++i) {
            const auto choose = (e & f) ^ (~e & g);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto big0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
            const auto big1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
            const auto temp1 = h + big1 + choose + round_constants[i] + words[i];
            const auto temp2 = big0 + majority;
            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }
        digest[0] += a; digest[1] += b; digest[2] += c; digest[3] += d;
        digest[4] += e; digest[5] += f; digest[6] += g; digest[7] += h;
    }

    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (const auto word : digest) {
        hex << std::setw(8) << word;
    }
    return hex.str();
}

} // namespace groot
