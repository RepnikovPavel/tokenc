// SPDX-License-Identifier: 0BSD
// Thin POSIX filesystem helpers used across tokenc. Kept header-only-ish with
// a small .cpp to centralize includes; nothing here depends on C++ std::filesystem
// (we walk dirent/stat directly for speed and predictable symlink behavior).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace tokenc::fs {

struct Stat {
    bool valid = false;          // whether stat succeeded
    bool is_directory = false;
    bool is_symlink = false;
    std::uint64_t size = 0;      // bytes
    std::uint64_t mtime_ns = 0;  // last modification time, nanoseconds
    std::uint64_t dev = 0;       // device id (for cache identity)
    std::uint64_t ino = 0;       // inode (for cache identity)
};

// lstat: information about the link itself (we do not follow symlinks during
// the walk, to avoid loops and counting generated files outside the tree).
[[nodiscard]] Stat lstat(std::string_view path);

// stat: information about the target (used to decide if a symlink points at a
// directory before descending).
[[nodiscard]] Stat stat(std::string_view path);

// Read a whole file into a string. Returns false on failure. Used for parsing
// ignore files; not for line counting (counter uses mmap).
[[nodiscard]] bool read_file(std::string_view path, std::string& out);

// True if a regular file exists and is readable.
[[nodiscard]] bool file_exists(std::string_view path);

// Join two path components with a single '/'.
[[nodiscard]] std::string join(std::string_view a, std::string_view b);

// Returns the canonical absolute path of `p` (resolves .. and ., does not
// require the target to exist). Falls back to `p` on failure.
[[nodiscard]] std::string absolute(std::string_view p);

// Get an XDG-style cache directory for tokenc, creating it if missing.
// Honors $XDG_CACHE_HOME, falls back to $HOME/.cache.
[[nodiscard]] std::string cache_dir();

}  // namespace tokenc::fs
