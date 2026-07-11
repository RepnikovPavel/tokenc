// SPDX-License-Identifier: 0BSD
#include "output.hpp"

#include <algorithm>
#include <sstream>
#include <string>

namespace tokenc {
namespace {

// Sort by lines descending, then language name ascending (deterministic).
std::vector<LanguageStats> sorted(std::vector<LanguageStats> in)
{
    std::sort(in.begin(), in.end(),
              [](const LanguageStats& a, const LanguageStats& b) {
                  if (a.lines != b.lines) return a.lines > b.lines;
                  return a.language < b.language;
              });
    return in;
}

}  // namespace

std::string render_tsv(const std::vector<LanguageStats>& stats_in,
                       std::uint64_t total_files,
                       std::uint64_t total_lines)
{
    const auto stats = sorted(stats_in);
    std::ostringstream out;
    out << "language\tfiles\tlines\n";
    for (const auto& s : stats)
        out << s.language << '\t' << s.files << '\t' << s.lines << '\n';
    out << "Total\t" << total_files << '\t' << total_lines << '\n';
    return out.str();
}

std::string render_table(const std::vector<LanguageStats>& stats_in,
                         std::uint64_t total_files,
                         std::uint64_t total_lines)
{
    const auto stats = sorted(stats_in);

    // Column widths.
    std::size_t w_lang = std::string("language").size();
    std::size_t w_files = std::string("files").size();
    std::size_t w_lines = std::string("lines").size();
    auto num_len = [](std::uint64_t n) {
        std::size_t l = 1;
        for (std::uint64_t x = n; x >= 10; x /= 10) ++l;
        return l;
    };
    for (const auto& s : stats) {
        w_lang = std::max(w_lang, s.language.size());
        w_files = std::max(w_files, num_len(s.files));
        w_lines = std::max(w_lines, num_len(s.lines));
    }
    w_files = std::max(w_files, num_len(total_files));
    w_lines = std::max(w_lines, num_len(total_lines));

    auto pad_l = [](const std::string& s, std::size_t w) {
        return s + std::string(w >= s.size() ? w - s.size() : 0, ' ');
    };
    auto pad_r = [](const std::string& s, std::size_t w) {
        return std::string(w >= s.size() ? w - s.size() : 0, ' ') + s;
    };

    std::ostringstream out;
    auto sep = [&]() {
        out << '+';
        out << std::string(w_lang + 2, '-');
        out << '+';
        out << std::string(w_files + 2, '-');
        out << '+';
        out << std::string(w_lines + 2, '-');
        out << "+\n";
    };
    auto row = [&](const std::string& lang, const std::string& files,
                   const std::string& lines) {
        out << "| " << pad_l(lang, w_lang)
            << " | " << pad_r(files, w_files)
            << " | " << pad_r(lines, w_lines) << " |\n";
    };

    sep();
    row("language", "files", "lines");
    sep();
    for (const auto& s : stats) {
        row(s.language, std::to_string(s.files), std::to_string(s.lines));
    }
    sep();
    row("Total", std::to_string(total_files), std::to_string(total_lines));
    sep();
    return out.str();
}

}  // namespace tokenc
