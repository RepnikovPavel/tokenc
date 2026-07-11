// SPDX-License-Identifier: MIT
#include "output.hpp"

#include <cstdio>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace tokenc {
namespace {

// Formats seconds with appropriate precision, e.g. "12.4 ms", "1.23 s".
std::string buf_format_seconds(double sec)
{
    char buf[32];
    if (sec < 1.0)
        std::snprintf(buf, sizeof(buf), "%.1f ms", sec * 1000.0);
    else if (sec < 60.0)
        std::snprintf(buf, sizeof(buf), "%.2f s", sec);
    else {
        const long m = static_cast<long>(sec) / 60;
        const long s = static_cast<long>(sec) % 60;
        std::snprintf(buf, sizeof(buf), "%ldm %lds", m, s);
    }
    return buf;
}

// 256-color-ish palette using SGR foreground codes. We keep it simple: a small
// fixed set of pleasant hues.
struct Ansi {
    static constexpr const char* kReset = "\033[0m";
    static constexpr const char* kBold = "\033[1m";
    static constexpr const char* kDim = "\033[2m";
    static constexpr const char* kCyan = "\033[36m";
    static constexpr const char* kGreen = "\033[32m";
    static constexpr const char* kYellow = "\033[33m";
    static constexpr const char* kBlue = "\033[34m";
    static constexpr const char* kMagenta = "\033[35m";
    static constexpr const char* kRed = "\033[31m";
    static constexpr const char* kWhite = "\033[97m";
    static constexpr const char* kGrey = "\033[90m";
};

// Group thousands with an underscore-free ASCII grouping (thin space is not
// universally rendered in terminals, so we use plain comma grouping). Returns
// e.g. "1,234,567".
std::string group_thousands(std::uint64_t n)
{
    std::string s = std::to_string(n);
    std::string out;
    const std::size_t len = s.size();
    for (std::size_t i = 0; i < len; ++i) {
        if (i > 0 && (len - i) % 3 == 0)
            out.push_back(',');
        out.push_back(s[i]);
    }
    return out;
}

// Cycle through a small palette for language rows so the table is readable.
const char* row_color(std::size_t idx)
{
    static constexpr const char* kColors[] = {
        Ansi::kCyan, Ansi::kGreen, Ansi::kYellow, Ansi::kBlue,
        Ansi::kMagenta, Ansi::kRed, Ansi::kWhite,
    };
    return kColors[idx % (sizeof(kColors) / sizeof(kColors[0]))];
}

std::size_t display_width(const std::string& s)
{
    // Approximate: count UTF-8 leading bytes (non-continuation bytes). All our
    // language names are ASCII, so this is exact for them.
    std::size_t w = 0;
    for (unsigned char c : s)
        if (c < 0x80 || c >= 0xC0)
            ++w;
    return w;
}

// Pad `s` on the right to at least `width` columns with spaces.
std::string pad_right(const std::string& s, std::size_t width)
{
    const std::size_t w = display_width(s);
    if (w >= width)
        return s;
    return s + std::string(width - w, ' ');
}

std::string pad_left(const std::string& s, std::size_t width)
{
    const std::size_t w = display_width(s);
    if (w >= width)
        return s;
    return std::string(width - w, ' ') + s;
}

}  // namespace

ColorMode resolve_color(ColorMode mode)
{
    if (mode == ColorMode::Auto)
        return ::isatty(STDOUT_FILENO) ? ColorMode::Always : ColorMode::Never;
    return mode;
}

std::string render_report(const std::vector<LanguageStats>& stats_in,
                          std::uint64_t total_files,
                          std::uint64_t total_lines,
                          double elapsed_sec,
                          const OutputOptions& opts)
{
    const bool color = resolve_color(opts.color) == ColorMode::Always;

    std::vector<LanguageStats> stats = stats_in;
    switch (opts.sort) {
        case SortMode::Lines:
            std::sort(stats.begin(), stats.end(),
                      [](const LanguageStats& a, const LanguageStats& b) {
                          if (a.lines != b.lines) return a.lines > b.lines;
                          return a.language < b.language;
                      });
            break;
        case SortMode::Files:
            std::sort(stats.begin(), stats.end(),
                      [](const LanguageStats& a, const LanguageStats& b) {
                          if (a.files != b.files) return a.files > b.files;
                          return a.language < b.language;
                      });
            break;
        case SortMode::Name:
            std::sort(stats.begin(), stats.end(),
                      [](const LanguageStats& a, const LanguageStats& b) {
                          return a.language < b.language;
                      });
            break;
    }

    // Column widths.
    std::size_t w_lang = std::string_view("Language").size();
    std::size_t w_files = std::string_view("Files").size();
    std::size_t w_lines = std::string_view("Lines").size();
    std::size_t w_pct = std::string_view("Lines %").size();
    for (const auto& s : stats) {
        w_lang = std::max(w_lang, display_width(s.language));
        w_files = std::max(w_files, group_thousands(s.files).size());
        w_lines = std::max(w_lines, group_thousands(s.lines).size());
    }
    const std::string total_files_s = group_thousands(total_files);
    const std::string total_lines_s = group_thousands(total_lines);
    w_files = std::max(w_files, total_files_s.size());
    w_lines = std::max(w_lines, total_lines_s.size());

    const std::size_t total_width = 2 + w_lang + 3 + w_files + 3 + w_lines + 3 + w_pct;

    std::ostringstream out;

    // Header.
    if (color)
        out << Ansi::kBold << Ansi::kGrey;
    out << "  " << pad_right("Language", w_lang) << "   "
        << pad_left("Files", w_files) << "   "
        << pad_left("Lines", w_lines) << "   "
        << pad_left("Lines %", w_pct);
    if (color)
        out << Ansi::kReset;
    out << '\n';
    if (color)
        out << Ansi::kGrey;
    out << std::string(total_width, '-');
    if (color)
        out << Ansi::kReset;
    out << '\n';

    // Rows.
    for (std::size_t i = 0; i < stats.size(); ++i) {
        const auto& s = stats[i];
        const double pct = (total_lines == 0) ? 0.0
                            : (100.0 * static_cast<double>(s.lines)) / static_cast<double>(total_lines);
        // Format percentage with one decimal, but show "100.0" exactly when applicable.
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.1f", pct);
        const std::string pct_s = buf;

        if (color) {
            const char* c = row_color(i);
            out << "  " << c << pad_right(s.language, w_lang) << Ansi::kReset
                << "   " << Ansi::kBold << pad_left(group_thousands(s.files), w_files) << Ansi::kReset
                << "   " << Ansi::kBold << pad_left(group_thousands(s.lines), w_lines) << Ansi::kReset
                << "   " << Ansi::kGrey << pad_left(pct_s, w_pct) << Ansi::kReset;
        } else {
            out << "  " << pad_right(s.language, w_lang) << "   "
                << pad_left(group_thousands(s.files), w_files) << "   "
                << pad_left(group_thousands(s.lines), w_lines) << "   "
                << pad_left(pct_s, w_pct);
        }
        out << '\n';
    }

    // Separator + total.
    if (color)
        out << Ansi::kGrey;
    out << std::string(total_width, '-');
    if (color)
        out << Ansi::kReset;
    out << '\n';

    if (color)
        out << "  " << Ansi::kBold << pad_right("Total", w_lang) << Ansi::kReset
            << "   " << Ansi::kBold << pad_left(total_files_s, w_files) << Ansi::kReset
            << "   " << Ansi::kBold << Ansi::kGreen << pad_left(total_lines_s, w_lines) << Ansi::kReset
            << "   " << Ansi::kGrey << pad_left("100.0", w_pct) << Ansi::kReset;
    else
        out << "  " << pad_right("Total", w_lang) << "   "
            << pad_left(total_files_s, w_files) << "   "
            << pad_left(total_lines_s, w_lines) << "   "
            << pad_left("100.0", w_pct);
    out << '\n';

    // Summary line.
    if (opts.show_summary && !stats.empty()) {
        const double sec = elapsed_sec;
        out << '\n';
        if (color)
            out << Ansi::kGrey;
        out << stats.size() << " language"
            << (stats.size() == 1 ? "" : "s") << ", "
            << total_files_s << " file" << (total_files == 1 ? "" : "s") << ", "
            << total_lines_s << " line" << (total_lines == 1 ? "" : "s");
        if (sec >= 0.0)
            out << "  ·  " << buf_format_seconds(sec);
        if (color)
            out << Ansi::kReset;
        out << '\n';
    } else if (opts.show_summary && stats.empty()) {
        out << '\n';
        if (color)
            out << Ansi::kYellow;
        out << "No source files found under the given path.";
        if (color)
            out << Ansi::kReset;
        out << '\n';
    }

    return out.str();
}

}  // namespace tokenc
