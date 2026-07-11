// SPDX-License-Identifier: 0BSD
#include "output.hpp"

#include <algorithm>
#include <cstdio>
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

    // Format a per-language share of total_lines as a human-readable percentage.
    //   >= 10%      -> one decimal  (e.g. "82.4%")
    //   1% .. 10%  -> one decimal  (e.g. "5.7%")
    //   < 1%       -> two decimals (e.g. "0.04%"), but "<0.01%" if truly tiny
    //   total == 0 -> "-"          (avoid division by zero)
    auto fmt_pct = [&](std::uint64_t lines) -> std::string {
        if (total_lines == 0)
            return "-";
        double pct = 100.0 * static_cast<double>(lines) / static_cast<double>(total_lines);
        if (pct > 0.0 && pct < 0.01)
            return "<0.01%";
        char buf[16];
        if (pct >= 1.0)
            std::snprintf(buf, sizeof(buf), "%.1f%%", pct);
        else
            std::snprintf(buf, sizeof(buf), "%.2f%%", pct);
        return buf;
    };

    // Pre-format the percentage column so its width can be measured.
    std::vector<std::string> pct_col;
    pct_col.reserve(stats.size() + 1);
    for (const auto& s : stats)
        pct_col.push_back(fmt_pct(s.lines));
    pct_col.push_back("100.0%");  // Total row

    // Column widths.
    std::size_t w_lang = std::string("language").size();
    std::size_t w_files = std::string("files").size();
    std::size_t w_lines = std::string("lines").size();
    std::size_t w_pct = std::string("lines %").size();
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
    for (const auto& p : pct_col)
        w_pct = std::max(w_pct, p.size());

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
        out << '+';
        out << std::string(w_pct + 2, '-');
        out << "+\n";
    };
    auto row = [&](const std::string& lang, const std::string& files,
                   const std::string& lines, const std::string& pct) {
        out << "| " << pad_l(lang, w_lang)
            << " | " << pad_r(files, w_files)
            << " | " << pad_r(lines, w_lines)
            << " | " << pad_r(pct, w_pct) << " |\n";
    };

    sep();
    row("language", "files", "lines", "lines %");
    sep();
    for (std::size_t i = 0; i < stats.size(); ++i) {
        row(stats[i].language, std::to_string(stats[i].files),
            std::to_string(stats[i].lines), pct_col[i]);
    }
    sep();
    row("Total", std::to_string(total_files), std::to_string(total_lines), "100.0%");
    sep();
    return out.str();
}

}  // namespace tokenc
