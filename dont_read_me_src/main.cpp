// SPDX-License-Identifier: 0BSD
// tokenc — fast recursive line counter for source code.
//
//   tokenc [options] [path]
//
//   path                  Directory or file to count (default: .)
//   --no-gitignore        Ignore .gitignore/.ignore/.dockerignore files
//   --no-default-ignore   Disable the built-in non-code ignore list
//   --include-generated   Count machine-generated files (excluded by default)
//   --all                 Descend into git submodules (excluded by default)
//   --no-cache            Do not read or write the line cache
//   --format              Pretty psql-like table (default is machine TSV)
//   -j, --jobs=N          Worker threads (default: hardware concurrency)
//   -h, --help            Show help and exit
//   -v, --version         Print version and exit

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
        "tokenc — line counter for source code\n"
        "\n"
        "Usage: tokenc [options] [path]\n"
        "\n"
        "  Counts physical lines of recognized source files in a directory tree.\n"
        "  Respects .gitignore and a built-in non-code ignore list.\n"
        "  Machine-generated files (protoc/gRPC/thrift/codegen output) are excluded.\n"
        "  Output is TSV: language<TAB>files<TAB>lines, plus a Total row.\n"
        "\n"
        "Options:\n"
        "  path                    Directory or file to count (default: .)\n"
        "      --no-gitignore      Ignore .gitignore/.ignore/.dockerignore files\n"
        "      --no-default-ignore Disable the built-in non-code ignore list\n"
        "      --include-generated Count machine-generated files (off by default)\n"
        "      --all               Descend into git submodules (off by default)\n"
        "      --no-cache          Do not read or write the line cache\n"
        "      --format            Pretty psql-like table instead of TSV\n"
        "  -j, --jobs=N            Worker threads (default: auto)\n"
        "  -h, --help              Show this help and exit\n"
        "  -v, --version           Print version and exit\n",
        stdout);
}

bool starts_with(std::string_view s, std::string_view prefix)
{
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// Parse a non-negative integer from a string; returns false on error.
bool parse_uint(std::string_view s, unsigned& out)
{
    if (s.empty()) return false;
    unsigned n = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        n = n * 10 + static_cast<unsigned>(c - '0');
    }
    out = n;
    return true;
}

}  // namespace

int main(int argc, char** argv)
{
    std::string path = ".";
    bool use_gitignore = true;
    bool use_default_ignore = true;
    bool use_cache = true;
    bool include_generated = false;   // generated code excluded by default
    bool include_submodules = false;  // git submodules skipped by default
    bool pretty_format = false;       // default output is machine TSV
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
        } else if (arg == "--include-generated") {
            include_generated = true;
        } else if (arg == "--all") {
            include_submodules = true;
        } else if (arg == "--format") {
            pretty_format = true;
        } else if (arg == "--no-cache") {
            use_cache = false;
        } else if (arg == "-j" || arg == "--jobs") {
            if (i + 1 >= argc) { std::fprintf(stderr, "tokenc: --jobs requires an argument\n"); return 2; }
            if (!parse_uint(argv[++i], jobs)) { std::fprintf(stderr, "tokenc: invalid --jobs value\n"); return 2; }
        } else if (starts_with(arg, "--jobs=")) {
            if (!parse_uint(arg.substr(7), jobs)) { std::fprintf(stderr, "tokenc: invalid --jobs value\n"); return 2; }
        } else if (starts_with(arg, "-j") && arg.size() > 2) {
            if (!parse_uint(arg.substr(2), jobs)) { std::fprintf(stderr, "tokenc: invalid -j value\n"); return 2; }
        } else if (!arg.empty() && arg[0] == '-') {
            std::fprintf(stderr, "tokenc: unknown option: %s\nTry 'tokenc --help'.\n", argv[i]);
            return 2;
        } else {
            if (path_set) { std::fprintf(stderr, "tokenc: unexpected extra argument: %s\n", argv[i]); return 2; }
            path = arg;
            path_set = true;
        }
    }

    const tokenc::fs::Stat target_st = tokenc::fs::stat(path);
    if (!target_st.valid) {
        std::fprintf(stderr, "tokenc: cannot access '%s': %s\n", path.c_str(), std::strerror(errno));
        return 1;
    }

    std::vector<tokenc::FoundFile> files =
        tokenc::walk(path, use_gitignore, use_default_ignore, include_submodules);

    std::unique_ptr<tokenc::LineCache> cache;
    if (use_cache)
        cache = std::make_unique<tokenc::LineCache>(tokenc::fs::cache_dir());

    std::uint64_t total_files = 0;
    std::uint64_t total_lines = 0;
    std::vector<tokenc::LanguageStats> stats =
        tokenc::count_all(files, cache.get(), jobs, total_files, total_lines,
                          /*skip_generated=*/!include_generated);

    if (cache) cache->flush();

    const std::string report = pretty_format
        ? tokenc::render_table(stats, total_files, total_lines)
        : tokenc::render_tsv(stats, total_files, total_lines);
    std::fputs(report.c_str(), stdout);
    return 0;
}
