// SPDX-License-Identifier: MIT
// tokenc — fast recursive line counter for source code.
//
// Usage:
//   tokenc [options] [path]
//
//   path            Directory (or single file) to count. Defaults to ".".
//
// Options:
//   --no-gitignore        Ignore .gitignore / .ignore / .dockerignore files.
//   --no-default-ignore   Disable the built-in non-code ignore list.
//   --no-cache            Do not read or write the line cache.
//   --no-color            Disable ANSI colors.
//   --color               Force ANSI colors on (even when piped).
//   --sort=lines|files|name   Sort order (default: lines).
//   -j, --jobs=N          Worker threads (default: hardware concurrency).
//   -h, --help            Show this help and exit.
//   -v, --version         Print version and exit.
//
// Build: see ../CMakeLists.txt and ../install.sh.

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "cache.hpp"
#include "counter.hpp"
#include "fs.hpp"
#include "output.hpp"
#include "walker.hpp"

namespace {

constexpr const char* kVersion = "tokenc 1.0.0";

void print_help()
{
    std::fputs(
        "tokenc — fast line counter for source code\n"
        "\n"
        "Usage: tokenc [options] [path]\n"
        "\n"
        "  Count lines of code in a directory tree (or a single file), recursively.\n"
        "  Only recognized source files are counted; .gitignore rules and a built-in\n"
        "  non-code ignore list are respected.\n"
        "\n"
        "Options:\n"
        "  path                       Directory or file to count (default: .)\n"
        "      --no-gitignore         Ignore .gitignore/.ignore/.dockerignore files\n"
        "      --no-default-ignore    Disable the built-in non-code ignore list\n"
        "      --no-cache             Do not read or write the line cache\n"
        "      --no-color             Disable ANSI colors\n"
        "      --color                Force ANSI colors on\n"
        "      --sort=lines|files|name   Sort order (default: lines)\n"
        "  -j, --jobs=N               Worker threads (default: auto)\n"
        "  -h, --help                 Show this help and exit\n"
        "  -v, --version              Print version and exit\n"
        "\n"
        "Examples:\n"
        "  tokenc .                    # count current directory\n"
        "  tokenc src                  # count a subdirectory\n"
        "  tokenc --no-color src | tee report.txt\n",
        stdout);
}

bool starts_with(std::string_view s, std::string_view prefix)
{
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

}  // namespace

int main(int argc, char** argv)
{
    std::string path = ".";
    bool use_gitignore = true;
    bool use_default_ignore = true;
    bool use_cache = true;
    tokenc::OutputOptions out_opts;
    out_opts.color = tokenc::ColorMode::Auto;
    out_opts.sort = tokenc::SortMode::Lines;
    out_opts.show_summary = true;
    unsigned jobs = 0;
    bool path_set = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "-h" || arg == "--help") {
            print_help();
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::puts(kVersion);
            return 0;
        } else if (arg == "--no-gitignore") {
            use_gitignore = false;
        } else if (arg == "--no-default-ignore") {
            use_default_ignore = false;
        } else if (arg == "--no-cache") {
            use_cache = false;
        } else if (arg == "--no-color") {
            out_opts.color = tokenc::ColorMode::Never;
        } else if (arg == "--color") {
            out_opts.color = tokenc::ColorMode::Always;
        } else if (arg == "--sort") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "tokenc: --sort requires an argument\n");
                return 2;
            }
            const std::string v(argv[++i]);
            if (v == "lines") out_opts.sort = tokenc::SortMode::Lines;
            else if (v == "files") out_opts.sort = tokenc::SortMode::Files;
            else if (v == "name") out_opts.sort = tokenc::SortMode::Name;
            else {
                std::fprintf(stderr, "tokenc: invalid --sort value: %s\n", v.c_str());
                return 2;
            }
        } else if (starts_with(arg, "--sort=")) {
            const std::string v(arg.substr(7));
            if (v == "lines") out_opts.sort = tokenc::SortMode::Lines;
            else if (v == "files") out_opts.sort = tokenc::SortMode::Files;
            else if (v == "name") out_opts.sort = tokenc::SortMode::Name;
            else {
                std::fprintf(stderr, "tokenc: invalid --sort value: %s\n", v.c_str());
                return 2;
            }
        } else if (arg == "-j" || arg == "--jobs") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "tokenc: --jobs requires an argument\n");
                return 2;
            }
            const std::string v(argv[++i]);
            try {
                long n = std::stol(v);
                if (n < 0) throw std::out_of_range("neg");
                jobs = static_cast<unsigned>(n);
            } catch (...) {
                std::fprintf(stderr, "tokenc: invalid --jobs value: %s\n", v.c_str());
                return 2;
            }
        } else if (starts_with(arg, "--jobs=")) {
            const std::string v(arg.substr(7));
            try {
                long n = std::stol(v);
                if (n < 0) throw std::out_of_range("neg");
                jobs = static_cast<unsigned>(n);
            } catch (...) {
                std::fprintf(stderr, "tokenc: invalid --jobs value: %s\n", v.c_str());
                return 2;
            }
        } else if (starts_with(arg, "-j")) {
            const std::string v(arg.substr(2));
            try {
                long n = std::stol(v);
                if (n < 0) throw std::out_of_range("neg");
                jobs = static_cast<unsigned>(n);
            } catch (...) {
                std::fprintf(stderr, "tokenc: invalid -j value: %s\n", v.c_str());
                return 2;
            }
        } else if (arg.size() > 0 && arg[0] == '-') {
            std::fprintf(stderr, "tokenc: unknown option: %s\n", argv[i]);
            std::fprintf(stderr, "Try 'tokenc --help' for usage.\n");
            return 2;
        } else {
            if (path_set) {
                std::fprintf(stderr, "tokenc: unexpected extra argument: %s\n", argv[i]);
                return 2;
            }
            path = arg;
            path_set = true;
        }
    }

    // Verify the target path is at least accessible; walker handles the rest.
    const tokenc::fs::Stat target_st = tokenc::fs::stat(path);
    if (!target_st.valid) {
        std::fprintf(stderr, "tokenc: cannot access '%s': %s\n",
                     path.c_str(), std::strerror(errno));
        return 1;
    }

    const auto t_start = std::chrono::steady_clock::now();

    // Walk the tree.
    std::vector<tokenc::FoundFile> files =
        tokenc::walk(path, use_gitignore, use_default_ignore);

    // Count lines (optionally with cache).
    std::unique_ptr<tokenc::LineCache> cache;
    if (use_cache)
        cache = std::make_unique<tokenc::LineCache>(tokenc::fs::cache_dir());

    std::uint64_t total_files = 0;
    std::uint64_t total_lines = 0;
    std::vector<tokenc::LanguageStats> stats =
        tokenc::count_all(files, cache.get(), jobs, total_files, total_lines);

    const auto t_end = std::chrono::steady_clock::now();
    const double elapsed_sec =
        std::chrono::duration<double>(t_end - t_start).count();

    // Persist cache after counting (best-effort).
    if (cache)
        cache->flush();

    // Render and print.
    const std::string report =
        tokenc::render_report(stats, total_files, total_lines, elapsed_sec, out_opts);
    std::fputs(report.c_str(), stdout);

    return 0;
}
