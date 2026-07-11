// SPDX-License-Identifier: 0BSD
#include "output.hpp"

#include <algorithm>
#include <sstream>

namespace tokenc {

std::string render_tsv(const std::vector<LanguageStats>& stats_in,
                       std::uint64_t total_files,
                       std::uint64_t total_lines)
{
    std::vector<LanguageStats> stats = stats_in;
    // Sort by lines desc, then language name asc — deterministic output.
    std::sort(stats.begin(), stats.end(),
              [](const LanguageStats& a, const LanguageStats& b) {
                  if (a.lines != b.lines) return a.lines > b.lines;
                  return a.language < b.language;
              });

    std::ostringstream out;
    out << "language\tfiles\tlines\n";
    for (const auto& s : stats)
        out << s.language << '\t' << s.files << '\t' << s.lines << '\n';
    out << "Total\t" << total_files << '\t' << total_lines << '\n';
    return out.str();
}

}  // namespace tokenc
