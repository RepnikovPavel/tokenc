// SPDX-License-Identifier: 0BSD
// Recursive directory walker that respects ignore rules.
//
// The walk is depth-first. At each directory we look for an ignore file
// (.gitignore / .ignore / .dockerignore / .npmignore) and push its compiled
// patterns onto an IgnoreStack scoped to that directory. When we leave the
// directory, the set is popped. This mirrors git's nested-.gitignore behavior.
//
// The walker collects all files recognized as source code (see languages.hpp).
// Symlinks are never followed, to avoid escaping the scanned tree.

#pragma once

#include <string>
#include <vector>

#include "fs.hpp"
#include "gitignore.hpp"

namespace tokenc {

struct Options;  // forward decl (defined in main.cpp via a shared header)

// A file discovered during the walk, together with the stat info needed for
// cache identity and (later) line counting.
struct FoundFile {
    std::string path;          // absolute path
    std::string language;      // language name (from languages.hpp)
    fs::Stat stat;             // lstat result for cache key & size
};

// Walk `root` and return every source file not excluded by ignore rules.
// `use_gitignore` controls whether .gitignore-style files are honored;
// `use_default_ignore` toggles the built-in non-code ignore list.
std::vector<FoundFile> walk(const std::string& root,
                            bool use_gitignore,
                            bool use_default_ignore);

}  // namespace tokenc
