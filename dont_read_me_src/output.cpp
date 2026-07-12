// SPDX-License-Identifier: 0BSD
#include "output.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>

namespace tokenc {
namespace {

std::vector<LanguageStats> sorted(std::vector<LanguageStats> in)
{
    std::sort(in.begin(), in.end(),
              [](const LanguageStats& a, const LanguageStats& b) {
                  if (a.lines != b.lines) return a.lines > b.lines;
                  return a.language < b.language;
              });
    return in;
}

std::size_t num_len(std::uint64_t n)
{
    std::size_t l = 1;
    for (std::uint64_t x = n; x >= 10; x /= 10) ++l;
    return l;
}

std::string fmt_pct(std::uint64_t part, std::uint64_t total)
{
    if (total == 0)
        return "-";
    const double pct = 100.0 * static_cast<double>(part) / static_cast<double>(total);
    if (pct > 0.0 && pct < 0.01)
        return "<0.01%";
    char buf[16];
    if (pct >= 1.0)
        std::snprintf(buf, sizeof(buf), "%.1f%%", pct);
    else
        std::snprintf(buf, sizeof(buf), "%.2f%%", pct);
    return buf;
}

// Machine TSV: numeric percent, one decimal (no '%' suffix).
std::string fmt_pct_num(std::uint64_t part, std::uint64_t total)
{
    if (total == 0)
        return "0";
    const double pct = 100.0 * static_cast<double>(part) / static_cast<double>(total);
    char buf[16];
    if (pct >= 1.0)
        std::snprintf(buf, sizeof(buf), "%.1f", pct);
    else if (pct > 0.0 && pct < 0.01)
        std::snprintf(buf, sizeof(buf), "0.0");
    else
        std::snprintf(buf, sizeof(buf), "%.2f", pct);
    return buf;
}

}  // namespace

std::string render_tsv(const std::vector<LanguageStats>& stats_in,
                       const CountTotals& totals,
                       bool include_tokens)
{
    const auto stats = sorted(stats_in);
    std::ostringstream out;
    if (include_tokens)
        out << "language\tfiles\tlines\tcl100k_base\to200k_base\tlines_pct\ttokens_pct\n";
    else
        out << "language\tfiles\tlines\n";
    for (const auto& s : stats) {
        out << s.language << '\t' << s.files << '\t' << s.lines;
        if (include_tokens) {
            out << '\t' << s.cl100k_base << '\t' << s.o200k_base
                << '\t' << fmt_pct_num(s.lines, totals.lines)
                << '\t' << fmt_pct_num(s.cl100k_base, totals.cl100k_base);
        }
        out << '\n';
    }
    out << "Total\t" << totals.files << '\t' << totals.lines;
    if (include_tokens)
        out << '\t' << totals.cl100k_base << '\t' << totals.o200k_base
            << '\t' << "100.0\t100.0";
    out << '\n';
    return out.str();
}

std::string render_table(const std::vector<LanguageStats>& stats_in,
                         const CountTotals& totals,
                         bool include_tokens)
{
    const auto stats = sorted(stats_in);

    std::vector<std::string> lines_pct_col;
    std::vector<std::string> tokens_pct_col;
    lines_pct_col.reserve(stats.size() + 1);
    tokens_pct_col.reserve(stats.size() + 1);
    for (const auto& s : stats) {
        lines_pct_col.push_back(fmt_pct(s.lines, totals.lines));
        tokens_pct_col.push_back(fmt_pct(s.cl100k_base, totals.cl100k_base));
    }
    lines_pct_col.push_back("100.0%");
    tokens_pct_col.push_back("100.0%");

    std::size_t w_lang = std::string("language").size();
    std::size_t w_files = std::string("files").size();
    std::size_t w_lines = std::string("lines").size();
    std::size_t w_cl = std::string("cl100k").size();
    std::size_t w_o2 = std::string("o200k").size();
    std::size_t w_lines_pct = std::string("lines %").size();
    std::size_t w_tokens_pct = std::string("tokens %").size();

    for (const auto& s : stats) {
        w_lang = std::max(w_lang, s.language.size());
        w_files = std::max(w_files, num_len(s.files));
        w_lines = std::max(w_lines, num_len(s.lines));
        if (include_tokens) {
            w_cl = std::max(w_cl, num_len(s.cl100k_base));
            w_o2 = std::max(w_o2, num_len(s.o200k_base));
        }
    }
    w_files = std::max(w_files, num_len(totals.files));
    w_lines = std::max(w_lines, num_len(totals.lines));
    if (include_tokens) {
        w_cl = std::max(w_cl, num_len(totals.cl100k_base));
        w_o2 = std::max(w_o2, num_len(totals.o200k_base));
    }
    for (const auto& p : lines_pct_col)
        w_lines_pct = std::max(w_lines_pct, p.size());
    for (const auto& p : tokens_pct_col)
        w_tokens_pct = std::max(w_tokens_pct, p.size());

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
        if (include_tokens) {
            out << '+';
            out << std::string(w_cl + 2, '-');
            out << '+';
            out << std::string(w_o2 + 2, '-');
            out << '+';
            out << std::string(w_lines_pct + 2, '-');
            out << '+';
            out << std::string(w_tokens_pct + 2, '-');
        }
        out << "+\n";
    };
    auto row = [&](const std::string& lang, const std::string& files,
                   const std::string& lines, const std::string& cl,
                   const std::string& o2, const std::string& lines_pct,
                   const std::string& tokens_pct) {
        out << "| " << pad_l(lang, w_lang)
            << " | " << pad_r(files, w_files)
            << " | " << pad_r(lines, w_lines);
        if (include_tokens) {
            out << " | " << pad_r(cl, w_cl)
                << " | " << pad_r(o2, w_o2)
                << " | " << pad_r(lines_pct, w_lines_pct)
                << " | " << pad_r(tokens_pct, w_tokens_pct);
        }
        out << " |\n";
    };

    sep();
    if (include_tokens)
        row("language", "files", "lines", "cl100k", "o200k", "lines %", "tokens %");
    else
        row("language", "files", "lines", "", "", "", "");
    sep();
    for (std::size_t i = 0; i < stats.size(); ++i) {
        row(stats[i].language, std::to_string(stats[i].files),
            std::to_string(stats[i].lines),
            std::to_string(stats[i].cl100k_base),
            std::to_string(stats[i].o200k_base),
            lines_pct_col[i], tokens_pct_col[i]);
    }
    sep();
    row("Total", std::to_string(totals.files), std::to_string(totals.lines),
        std::to_string(totals.cl100k_base), std::to_string(totals.o200k_base),
        "100.0%", "100.0%");
    sep();
    return out.str();
}

}  // namespace tokenc