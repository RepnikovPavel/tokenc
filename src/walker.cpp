// SPDX-License-Identifier: MIT
#include "walker.hpp"

#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "fs.hpp"
#include "languages.hpp"

namespace tokenc {
namespace {

// Recognized ignore-file basenames, in priority order (first found wins per
// directory, like ripgrep's .ignore taking precedence over .gitignore... but we
// simply merge all that are present, which is the safest superset behavior).
constexpr std::string_view kIgnoreFiles[] = {
    ".gitignore",
    ".ignore",
    ".dockerignore",
    ".npmignore",
    ".rgignore",
};

// Push an ignore set for `dir` if an ignore file lives there. Returns true if
// anything was pushed.
bool load_ignore_for_dir(const std::string& dir_abs, IgnoreStack& stack)
{
    bool pushed = false;
    for (const std::string_view name : kIgnoreFiles) {
        const std::string file = fs::join(dir_abs, std::string(name));
        std::string content;
        if (fs::read_file(file, content)) {
            IgnoreSet set(dir_abs);
            set.parse(content);
            if (!set.empty()) {
                stack.push(std::move(set));
                pushed = true;
            }
        }
    }
    return pushed;
}

void walk_dir(const std::string& dir_abs,
              IgnoreStack& stack,
              bool use_gitignore,
              std::vector<FoundFile>& out,
              unsigned depth)
{
    // Hard guard against pathological symlink loops or very deep trees.
    if (depth > 4096)
        return;

    bool pushed_here = false;
    if (use_gitignore)
        pushed_here = load_ignore_for_dir(dir_abs, stack);

    DIR* d = ::opendir(dir_abs.c_str());
    if (!d) {
        if (pushed_here)
            stack.pop();
        return;
    }

    // Collect entries first so we can sort for deterministic output ordering.
    struct Entry {
        std::string name;
        fs::Stat stat;
    };
    std::vector<Entry> entries;
    entries.reserve(64);

    struct dirent* ent;
    while ((ent = ::readdir(d)) != nullptr) {
        const std::string_view name(ent->d_name);
        if (name == "." || name == "..")
            continue;
        std::string full = fs::join(dir_abs, std::string(name));
        fs::Stat st = fs::lstat(full);
        if (!st.valid)
            continue;
        entries.push_back({std::string(name), st});
    }
    ::closedir(d);

    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) { return a.name < b.name; });

    for (const auto& e : entries) {
        const std::string full = fs::join(dir_abs, e.name);

        // Never follow symlinks: skip them entirely (both files and dirs).
        if (e.stat.is_symlink)
            continue;

        if (e.stat.is_directory) {
            if (stack.is_ignored(full, /*is_dir=*/true))
                continue;
            walk_dir(full, stack, use_gitignore, out, depth + 1);
        } else {
            if (stack.is_ignored(full, /*is_dir=*/false))
                continue;
            auto lang = language_for_path(full);
            if (!lang)
                continue;  // not a recognized source file
            out.push_back({full, std::string(*lang), e.stat});
        }
    }

    if (pushed_here)
        stack.pop();
}

}  // namespace

std::vector<FoundFile> walk(const std::string& root,
                            bool use_gitignore,
                            bool use_default_ignore)
{
    std::vector<FoundFile> out;

    const std::string root_abs = fs::absolute(root);
    const fs::Stat root_st = fs::stat(root_abs);
    if (!root_st.valid || !root_st.is_directory) {
        // Not a directory: maybe a single file was passed.
        const fs::Stat lst = fs::lstat(root_abs);
        if (lst.valid && !lst.is_directory && !lst.is_symlink) {
            if (auto lang = language_for_path(root_abs))
                out.push_back({root_abs, std::string(*lang), lst});
        }
        return out;
    }

    IgnoreStack stack;
    if (use_default_ignore) {
        IgnoreSet defaults(root_abs);
        for (const std::string& p : default_ignore_patterns())
            defaults.add_pattern(p);
        if (!defaults.empty())
            stack.push(std::move(defaults));
    }

    walk_dir(root_abs, stack, use_gitignore, out, 0);
    return out;
}

}  // namespace tokenc
