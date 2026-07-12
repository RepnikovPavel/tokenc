// SPDX-License-Identifier: 0BSD
// An on-disk cache of file identity -> line count, stored under the XDG cache
// directory. The cache is a best-effort optimization for repeated runs over the
// same tree: if a file's (dev, ino, size, mtime_ns) all match, its previously
// counted line total is reused without re-reading the file.
//
// The cache is plain text (one record per line) so it is inspectable and
// trivial to delete. Writes are atomic (temp file + rename). All failures are
// silent: a missing/corrupt cache simply means we re-count everything.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "fs.hpp"

namespace tokenc {

class LineCache {
public:
    // Load from <cache_dir>/linecache.tsv. If the file cannot be read, the
    // cache is simply empty.
    explicit LineCache(std::string dir);

    // If the file identity matches a cached entry, set `out_lines` and return
    // true. Otherwise return false.
    bool lookup(const std::string& path, const fs::Stat& st, std::uint64_t& out_lines) const;

    // Full cache hit including token counts (v2 entries only).
    bool lookup_tokens(const std::string& path,
                       const fs::Stat& st,
                       std::uint64_t& out_lines,
                       std::uint64_t& out_cl100k,
                       std::uint64_t& out_o200k) const;

    // Remember a freshly computed line count for this file.
    void store(const std::string& path, const fs::Stat& st, std::uint64_t lines);

    void store_tokens(const std::string& path,
                      const fs::Stat& st,
                      std::uint64_t lines,
                      std::uint64_t cl100k,
                      std::uint64_t o200k);

    // Persist the in-memory cache to disk atomically. Safe to call once at the
    // end of a run. Returns false if the write failed (the program continues).
    bool flush();

private:
    struct Entry {
        std::uint64_t dev = 0;
        std::uint64_t ino = 0;
        std::uint64_t size = 0;
        std::uint64_t mtime_ns = 0;
        std::uint64_t lines = 0;
        std::uint64_t cl100k_base = 0;
        std::uint64_t o200k_base = 0;
        bool has_tokens = false;
    };

    std::string dir_;
    std::string file_;
    std::unordered_map<std::string, Entry> map_;
    bool dirty_ = false;
};

}  // namespace tokenc
