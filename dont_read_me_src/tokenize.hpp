// SPDX-License-Identifier: 0BSD
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace tokenc {

struct TokenCounts {
    std::uint64_t cl100k_base = 0;
    std::uint64_t o200k_base = 0;
};

// True if cl100k_base / o200k_base tokenization is available on this host.
[[nodiscard]] bool tokenizers_available();

// Tokenize many absolute file paths in one batch (external tiktoken helper).
// Missing paths are omitted from the result map.
[[nodiscard]] bool tokenize_files(const std::vector<std::string>& abs_paths,
                                  std::unordered_map<std::string, TokenCounts>& out);

}  // namespace tokenc