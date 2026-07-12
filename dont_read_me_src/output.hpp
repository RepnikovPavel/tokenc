// SPDX-License-Identifier: 0BSD
// Machine-readable TSV output. The whole point of tokenc is to feed its output
// to other programs / agents / LLMs, so output is fixed and easy to parse:
//
//   language<TAB>files<TAB>lines<TAB>cl100k_base<TAB>o200k_base
//   C++<TAB>8<TAB>1630
//   ...
//   Total<TAB>15<TAB>1977
//
// Languages are sorted by lines descending (tie-break: name asc). One trailing
// newline after each record. No colors, no decoration, no timings.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "counter.hpp"

namespace tokenc {

// Render the per-language report as TSV (header + one row per language + Total).
std::string render_tsv(const std::vector<LanguageStats>& stats,
                       const CountTotals& totals,
                       bool include_tokens);

// Render a pretty psql-style table with box-drawing borders, right-aligned
// numbers, sorted by lines descending. Intended for human reading (--format).
std::string render_table(const std::vector<LanguageStats>& stats,
                         const CountTotals& totals,
                         bool include_tokens);

}  // namespace tokenc
