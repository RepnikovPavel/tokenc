// SPDX-License-Identifier: 0BSD
#include "fs.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace tokenc::fs {
namespace {

Stat fill_stat(const struct ::stat& st)
{
    Stat s;
    s.valid = true;
    s.is_directory = S_ISDIR(st.st_mode);
    s.is_symlink = false;
    s.size = static_cast<std::uint64_t>(st.st_size);
#ifdef st_mtime
    s.mtime_ns = static_cast<std::uint64_t>(st.st_mtim.tv_sec) * 1000000000ull
               + static_cast<std::uint64_t>(st.st_mtim.tv_nsec);
#else
    s.mtime_ns = static_cast<std::uint64_t>(st.st_mtime) * 1000000000ull;
#endif
    s.dev = static_cast<std::uint64_t>(st.st_dev);
    s.ino = static_cast<std::uint64_t>(st.st_ino);
    return s;
}

// Resolve "." and ".." lexically without touching the filesystem.
std::string normalize_only(const std::string& path)
{
    const bool absolute = !path.empty() && path[0] == '/';
    std::vector<std::string> parts;
    parts.reserve(16);
    std::size_t i = 0;
    const std::size_t n = path.size();
    while (i < n) {
        while (i < n && path[i] == '/')
            ++i;
        if (i >= n)
            break;
        std::size_t j = path.find('/', i);
        if (j == std::string::npos)
            j = n;
        std::string seg = path.substr(i, j - i);
        i = j;
        if (seg == ".")
            continue;
        if (seg == "..") {
            if (!parts.empty() && parts.back() != "..")
                parts.pop_back();
            else if (!absolute)
                parts.push_back("..");
            continue;
        }
        parts.push_back(std::move(seg));
    }
    std::string out;
    if (absolute)
        out.push_back('/');
    for (std::size_t k = 0; k < parts.size(); ++k) {
        if (k)
            out.push_back('/');
        out += parts[k];
    }
    return out.empty() ? "." : out;
}

// Create a directory and any missing parents (like mkdir -p). Ignores errors;
// the caller checks existence separately when it matters.
void ensure_dir(const std::string& path)
{
    std::string acc;
    if (!path.empty() && path[0] == '/')
        acc.push_back('/');
    std::size_t i = 0;
    const std::size_t n = path.size();
    while (i < n) {
        while (i < n && path[i] == '/')
            ++i;
        if (i >= n)
            break;
        std::size_t j = path.find('/', i);
        if (j == std::string::npos)
            j = n;
        if (!acc.empty() && acc.back() != '/')
            acc.push_back('/');
        acc += path.substr(i, j - i);
        i = j;
        if (!acc.empty())
            ::mkdir(acc.c_str(), 0755);
    }
}

}  // namespace

Stat lstat(std::string_view path)
{
    Stat s;
    const std::string p(path);
    struct ::stat st;
    if (::lstat(p.c_str(), &st) != 0)
        return s;
    const bool is_link = S_ISLNK(st.st_mode);
    if (is_link) {
        s.valid = true;
        s.is_symlink = true;
        s.is_directory = false;
        s.size = static_cast<std::uint64_t>(st.st_size);
#ifdef st_mtime
        s.mtime_ns = static_cast<std::uint64_t>(st.st_mtim.tv_sec) * 1000000000ull
                   + static_cast<std::uint64_t>(st.st_mtim.tv_nsec);
#else
        s.mtime_ns = static_cast<std::uint64_t>(st.st_mtime) * 1000000000ull;
#endif
        s.dev = static_cast<std::uint64_t>(st.st_dev);
        s.ino = static_cast<std::uint64_t>(st.st_ino);
        return s;
    }
    return fill_stat(st);
}

Stat stat(std::string_view path)
{
    Stat s;
    const std::string p(path);
    struct ::stat st;
    if (::stat(p.c_str(), &st) != 0)
        return s;
    return fill_stat(st);
}

bool read_file(std::string_view path, std::string& out)
{
    const std::string p(path);
    std::ifstream f(p, std::ios::in | std::ios::binary);
    if (!f)
        return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool file_exists(std::string_view path)
{
    const std::string p(path);
    return ::access(p.c_str(), R_OK) == 0;
}

std::string join(std::string_view a, std::string_view b)
{
    if (a.empty())
        return std::string(b);
    if (b.empty())
        return std::string(a);
    std::string out;
    out.reserve(a.size() + 1 + b.size());
    out.append(a);
    if (out.back() != '/')
        out.push_back('/');
    out.append(b);
    return out;
}

std::string absolute(std::string_view p)
{
    std::string path(p);
    if (path.empty())
        path = ".";
    if (path[0] == '/')
        return normalize_only(path);
    char cwd[4096];
    if (::getcwd(cwd, sizeof(cwd)) == nullptr)
        return path;
    std::string joined = std::string(cwd);
    if (!joined.empty() && joined.back() != '/')
        joined.push_back('/');
    joined += path;
    return normalize_only(joined);
}

std::string cache_dir()
{
    const char* xdg = std::getenv("XDG_CACHE_HOME");
    if (xdg && xdg[0] != '\0') {
        std::string base = xdg;
        if (!base.empty() && base.back() == '/')
            base.pop_back();
        base += "/tokenc";
        ensure_dir(base);
        return base;
    }
    const char* home = std::getenv("HOME");
    std::string base = (home && home[0] != '\0')
                           ? std::string(home) + "/.cache/tokenc"
                           : ".tokenc-cache";
    ensure_dir(base);
    return base;
}

}  // namespace tokenc::fs
