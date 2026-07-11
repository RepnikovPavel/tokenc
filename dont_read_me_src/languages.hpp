// SPDX-License-Identifier: 0BSD
// Language classification: file extension -> language name, plus the built-in
// "default ignore" list of obvious non-code files and directories.
//
// tokenc only counts files it recognizes as source code. Configuration,
// markup, data and lock files are intentionally excluded even when not listed
// in any ignore file, because they do not represent "useful payload" (code).

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tokenc {

// Returns the language name for a path if its extension is a known source
// language, otherwise std::nullopt (meaning: not code, skip it).
// Lookup is O(1) via an internal hash map; case-insensitive on the extension.
std::optional<std::string_view> language_for_path(std::string_view path);

// The built-in patterns that are always ignored unless --no-default-ignore is
// given. These cover version-control dirs, language build/cache/output dirs,
// compiled artifacts, minified bundles and dependency lock files. Each pattern
// follows .gitignore syntax (so e.g. "*.pyc" or "node_modules").
const std::vector<std::string>& default_ignore_patterns();

// Filename -> line count entries are grouped by these language names in output.

}  // namespace tokenc
