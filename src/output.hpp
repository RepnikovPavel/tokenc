// SPDX-License-Identifier: MIT
// Pretty-printing of per-language line counts.
//
// Output is a fixed-width table, right-aligned numbers, with a trailing summary
// line. ANSI colors are emitted only when explicitly requested (the caller
// decides based on isatty(1) and the --color/--no-color flags).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "counter.hpp"

namespace tokenc {

enum class ColorMode { Auto, Always, Never };

enum class SortMode { Lines, Files, Name };

struct OutputOptions {
    ColorMode color = ColorMode::Auto;
    SortMode sort = SortMode::Lines;
    bool show_summary = true;
};

// Render the per-language table plus totals to the given stream. `elapsed_sec`
// is included in the summary line when present and non-negative.
std::string render_report(const std::vector<LanguageStats>& stats,
                          std::uint64_t total_files,
                          std::uint64_t total_lines,
                          double elapsed_sec,
                          const OutputOptions& opts);

// Resolve ColorMode::Auto against stdout's TTY status.
ColorMode resolve_color(ColorMode mode);

}  // namespace tokenc
