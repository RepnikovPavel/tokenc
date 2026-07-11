// SPDX-License-Identifier: MIT
#include "cache.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "fs.hpp"

namespace tokenc {
namespace {

// We use a tab-separated, line-oriented record format. Paths are stored
// verbatim; since tokenc only walks real source files under the scanned root,
// paths here never contain a newline.
//
//   <path>\t<dev>\t<ino>\t<size>\t<mtime_ns>\t<lines>\n
//
// A leading magic line marks the format version for forward compatibility.
constexpr const char* kMagic = "# tokenc linecache v1";

}  // namespace

LineCache::LineCache(std::string dir) : dir_(std::move(dir))
{
    file_ = fs::join(dir_, "linecache.tsv");
    std::string content;
    if (!fs::read_file(file_, content))
        return;
    std::istringstream in(content);
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (line.empty())
            continue;
        if (first) {
            first = false;
            if (line == kMagic)
                continue;  // recognized header
            // Not our header: fall through and still try to parse this line.
        }
        // Parse fields split by '\t'.
        Entry e;
        std::string path;
        std::istringstream ls(line);
        std::string field;
        std::vector<std::string> fields;
        fields.reserve(6);
        while (std::getline(ls, field, '\t'))
            fields.push_back(field);
        if (fields.size() != 6)
            continue;
        try {
            path = fields[0];
            e.dev = std::stoull(fields[1]);
            e.ino = std::stoull(fields[2]);
            e.size = std::stoull(fields[3]);
            e.mtime_ns = std::stoull(fields[4]);
            e.lines = std::stoull(fields[5]);
        } catch (...) {
            continue;
        }
        map_[std::move(path)] = e;
    }
}

bool LineCache::lookup(const std::string& path, const fs::Stat& st, std::uint64_t& out_lines) const
{
    const auto it = map_.find(path);
    if (it == map_.end())
        return false;
    const Entry& e = it->second;
    if (e.dev != st.dev || e.ino != st.ino || e.size != st.size || e.mtime_ns != st.mtime_ns)
        return false;
    out_lines = e.lines;
    return true;
}

void LineCache::store(const std::string& path, const fs::Stat& st, std::uint64_t lines)
{
    Entry e;
    e.dev = st.dev;
    e.ino = st.ino;
    e.size = st.size;
    e.mtime_ns = st.mtime_ns;
    e.lines = lines;
    map_[path] = e;
    dirty_ = true;
}

bool LineCache::flush()
{
    if (!dirty_)
        return true;
    // Ensure the cache directory exists (create on demand, ignore the returned
    // path; we already know it from the constructor).
    (void)fs::cache_dir();

    const std::string tmp = file_ + ".tmp";
    std::ofstream out(tmp, std::ios::out | std::ios::trunc);
    if (!out)
        return false;
    out << kMagic << '\n';
    for (const auto& [path, e] : map_) {
        out << path << '\t' << e.dev << '\t' << e.ino << '\t'
            << e.size << '\t' << e.mtime_ns << '\t' << e.lines << '\n';
    }
    out.flush();
    if (!out.good())
        return false;
    out.close();
    if (::rename(tmp.c_str(), file_.c_str()) != 0)
        return false;
    dirty_ = false;
    return true;
}

}  // namespace tokenc
