// SPDX-License-Identifier: MIT
#include "counter.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>

#include "cache.hpp"

namespace tokenc {
namespace {

// Count '\n' in [data, data+n). A trailing non-empty fragment without '\n'
// counts as one more line (matching `wc -l` for the newlines + 1 for missing
// final newline when the file has content but no trailing newline).
std::uint64_t count_newlines(const char* data, std::size_t n)
{
    if (n == 0)
        return 0;
    std::uint64_t nl = 0;
    const char* p = data;
    const char* end = data + n;
    // memchr is vectorized and much faster than a per-byte loop on big files.
    while (p < end) {
        const void* hit = std::memchr(p, '\n', static_cast<std::size_t>(end - p));
        if (!hit)
            break;
        ++nl;
        p = static_cast<const char*>(hit) + 1;
    }
    if (end[-1] != '\n')
        ++nl;  // a final line with no terminating newline
    return nl;
}

}  // namespace

std::uint64_t count_lines(const std::string& path)
{
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return 0;
    struct ::stat st;
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size == 0) {
        ::close(fd);
        return 0;
    }
    const std::size_t size = static_cast<std::size_t>(st.st_size);

    std::uint64_t lines = 0;
    void* mapped = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        // Fallback: read in chunks. (Should be rare.)
        std::string buf;
        buf.resize(1 << 16);
        std::size_t total_nl = 0;
        bool any = false;
        bool last_nl = false;
        while (true) {
            const ssize_t r = ::read(fd, buf.data(), buf.size());
            if (r <= 0)
                break;
            any = true;
            const char* p = buf.data();
            const char* e = p + r;
            while (p < e) {
                const void* hit = std::memchr(p, '\n', static_cast<std::size_t>(e - p));
                if (!hit) {
                    last_nl = false;
                    break;
                }
                ++total_nl;
                last_nl = true;
                p = static_cast<const char*>(hit) + 1;
            }
        }
        lines = any ? total_nl + (last_nl ? 0 : 1) : 0;
    } else {
        // Advise the kernel we will read this sequentially.
        ::madvise(mapped, size, MADV_SEQUENTIAL);
        lines = count_newlines(static_cast<const char*>(mapped), size);
        ::munmap(mapped, size);
    }
    ::close(fd);
    return lines;
}

std::vector<LanguageStats> count_all(std::vector<FoundFile>& files,
                                     LineCache* cache,
                                     unsigned threads,
                                     std::uint64_t& total_files,
                                     std::uint64_t& total_lines)
{
    if (threads == 0) {
        const unsigned hw = std::thread::hardware_concurrency();
        threads = hw > 0 ? hw : 4;
    }
    if (threads > files.size())
        threads = static_cast<unsigned>(files.size());
    if (threads == 0)
        threads = 1;

    const std::size_t n = files.size();
    std::vector<std::uint64_t> counts(n, 0);
    std::vector<unsigned char> counted(n, 0);  // 1 if cache hit

    std::atomic<std::size_t> next{0};
    auto worker = [&]() {
        while (true) {
            const std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
            if (i >= n)
                break;
            // Try cache first.
            if (cache) {
                std::uint64_t cached = 0;
                if (cache->lookup(files[i].path, files[i].stat, cached)) {
                    counts[i] = cached;
                    counted[i] = 1;
                    continue;
                }
            }
            counts[i] = count_lines(files[i].path);
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (unsigned t = 0; t < threads; ++t)
        pool.emplace_back(worker);
    for (auto& th : pool)
        th.join();

    // Aggregate per language.
    std::unordered_map<std::string, LanguageStats> by_lang;
    by_lang.reserve(64);
    total_files = 0;
    total_lines = 0;
    for (std::size_t i = 0; i < n; ++i) {
        auto it = by_lang.find(files[i].language);
        if (it == by_lang.end()) {
            auto [ins, _] = by_lang.emplace(files[i].language,
                                            LanguageStats{files[i].language, 0, 0});
            it = ins;
        }
        it->second.files += 1;
        it->second.lines += counts[i];
        total_files += 1;
        total_lines += counts[i];

        // Update cache for entries we actually counted (not cache hits).
        if (cache && !counted[i])
            cache->store(files[i].path, files[i].stat, counts[i]);
    }

    std::vector<LanguageStats> result;
    result.reserve(by_lang.size());
    for (auto& [_, v] : by_lang)
        result.push_back(std::move(v));
    return result;
}

}  // namespace tokenc
