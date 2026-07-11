// SPDX-License-Identifier: 0BSD
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

// Marker files that identify a virtualenv / conda env / installed-packages tree
// regardless of the directory's name. If any of these is present, the whole
// directory is treated as third-party and never descended into.
bool is_virtualenv_root(const std::string& dir_abs)
{
    // pyvenv.cfg — created by venv/virtualenv/poetry/uv for every virtualenv.
    if (fs::file_exists(fs::join(dir_abs, "pyvenv.cfg")))
        return true;
    // conda-meta/ — created by conda for every environment.
    {
        struct ::stat st;
        const std::string p = fs::join(dir_abs, "conda-meta");
        if (::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            return true;
    }
    return false;
}

// A checked-out git submodule has a `.git` entry that is a FILE (a gitdir
// pointer like "gitdir: ../../.git/modules/..."), unlike a normal repo whose
// `.git` is a directory. We treat such a directory as the root of external
// (submodule) code and skip it wholesale.
bool is_submodule_root(const std::string& dir_abs)
{
    const std::string dotgit = fs::join(dir_abs, ".git");
    struct ::stat st;
    if (::lstat(dotgit.c_str(), &st) != 0)
        return false;
    // A regular file (or a symlink) named .git inside a directory is the
    // classic submodule marker. A .git directory means this is just a normal
    // nested git repo, which we don't treat as third-party on that basis alone.
    return S_ISREG(st.st_mode) || S_ISLNK(st.st_mode);
}

// Parse a .gitmodules file at `root` and add every "path = ..." entry to the
// given IgnoreSet, so checked-out submodules are excluded by their declared
// location (works even before .git is populated). Paths are relative to root.
void add_submodule_paths(const std::string& root, IgnoreSet& set)
{
    const std::string gm = fs::join(root, ".gitmodules");
    std::string content;
    if (!fs::read_file(gm, content))
        return;
    // Lines look like:   path = libs/fmtlib
    std::size_t pos = 0;
    while (pos < content.size()) {
        const auto nl = content.find('\n', pos);
        const std::string_view line = (nl == std::string_view::npos)
            ? std::string_view(content).substr(pos)
            : std::string_view(content).substr(pos, nl - pos);
        // Trim leading spaces.
        std::string_view l = line;
        while (!l.empty() && (l.front() == ' ' || l.front() == '\t' || l.front() == '\r'))
            l.remove_prefix(1);
        if (l.compare(0, 5, "path ") == 0 || l.compare(0, 5, "path\t") == 0 ||
            l.rfind("path ", 0) == 0 || l.rfind("path\t", 0) == 0) {
            // Find '=' and take the trimmed value after it.
            const auto eq = l.find('=');
            if (eq != std::string_view::npos) {
                std::string_view val = l.substr(eq + 1);
                while (!val.empty() && (val.front() == ' ' || val.front() == '\t'))
                    val.remove_prefix(1);
                while (!val.empty() && (val.back() == ' ' || val.back() == '\t' || val.back() == '\r'))
                    val.remove_suffix(1);
                if (!val.empty())
                    set.add_pattern(val);  // anchored relative to root (basename-only if no '/')
            }
        }
        if (nl == std::string_view::npos)
            break;
        pos = nl + 1;
    }
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
            // Virtualenv / installed-packages detection: a directory containing
            // pyvenv.cfg or conda-meta is a Python environment (possibly with an
            // arbitrary name), so skip it wholesale — its contents are never the
            // project's own code.
            if (is_virtualenv_root(full))
                continue;
            // Git submodule detection: a directory whose ".git" is a file (a
            // gitdir pointer) is a checked-out submodule = external code.
            if (is_submodule_root(full))
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
        // .gitmodules: exclude the locations declared for git submodules, so
        // checked-out external repositories are not counted as project code.
        add_submodule_paths(root_abs, defaults);
        if (!defaults.empty())
            stack.push(std::move(defaults));
    }

    walk_dir(root_abs, stack, use_gitignore, out, 0);
    return out;
}

}  // namespace tokenc
