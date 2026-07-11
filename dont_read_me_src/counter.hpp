// SPDX-License-Identifier: 0BSD
// Fast physical line counting using mmap and a thread pool.
//
// A "line" is a maximal run of bytes terminated by '\n'. A file that does not
// end with a newline still contributes a line for its trailing content (so a
// single non-empty byte counts as one line, matching `wc -l` for the newline
// count plus one for a missing final newline).
//
// Empty (zero-byte) files contribute zero lines.

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "walker.hpp"

namespace tokenc {

// Count the physical lines of one file via mmap. Returns 0 for empty or
// unreadable files. The path must be NUL-terminated (use std::string).
std::uint64_t count_lines(const std::string& path);

// Per-language aggregate totals.
struct LanguageStats {
    std::string language;
    std::uint64_t files = 0;
    std::uint64_t lines = 0;
};

// A cache of (dev,ino,mtime,size) -> lines for files we already counted.
// Forward-declared here so the counter can use it without including cache.hpp.
class LineCache;

// Count all files in parallel, optionally consulting/updating `cache`.
// `threads` <= 0 means auto (= hardware concurrency).
std::vector<LanguageStats> count_all(std::vector<FoundFile>& files,
                                     LineCache* cache,
                                     unsigned threads,
                                     std::uint64_t& total_files,
                                     std::uint64_t& total_lines);

}  // namespace tokenc
