// SPDX-License-Identifier: 0BSD
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tokenc {

using Rank = std::uint32_t;

// cl100k_base (GPT-4) — native C++ port of tiktoken CoreBPE (src/lib.rs).
class Cl100kEncoder {
public:
    [[nodiscard]] static const Cl100kEncoder& instance();

    [[nodiscard]] std::size_t count_tokens(std::string_view text) const;
    [[nodiscard]] std::vector<Rank> encode_ordinary(std::string_view text) const;

private:
    Cl100kEncoder();
    [[nodiscard]] Rank lookup(std::string_view key) const;
    [[nodiscard]] std::vector<Rank> byte_pair_encode(std::string_view piece) const;

    std::vector<std::string> token_bytes_;
    std::vector<Rank> token_ranks_;
};

}  // namespace tokenc