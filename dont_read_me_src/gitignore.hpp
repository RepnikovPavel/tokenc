// SPDX-License-Identifier: 0BSD
// A focused re-implementation of git's pathspec matching used by .gitignore.
//
// We implement the subset that matters for counting source lines:
//   - blank lines and lines starting with '#' are comments (ignored);
//   - a leading backslash escapes a leading '#' or '!' or other specials;
//   - a leading '!' negates a previous match (re-includes a path);
//   - a trailing '/' restricts the pattern to directories only;
//   - a leading '/' anchors the pattern to the ignore-file's directory;
//   - '*' matches anything except '/'; '?' matches any single char except '/';
//   - '[...]' character classes (with ranges and leading '!' negation);
//   - '**' matches any number of path components (including zero):
//        - leading "**/"  matches in any directory,
//        - "/**"  matches everything inside,
//        - "/**/" matches zero or more leading directories;
//   - a pattern containing no '/' matches the basename at any depth.
//
// One IgnoreSet corresponds to a single .gitignore file located at `base`.
// Matching is performed against paths expressed RELATIVE to that base.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tokenc {

// Result of consulting an ignore set for a single path.
enum class Match : int {
    None = 0,      // no pattern matched
    Ignored = 1,   // a positive pattern matched last
    Unignored = 2, // a '!' negation matched last
};

struct IgnorePattern {
    std::string raw;          // pattern text after unescape/de-negate/trim
    bool negate = false;      // '!' re-include
    bool dir_only = false;    // trailing '/'
    bool anchored = false;    // leading '/' -> match from base only
    bool match_basename = false;  // no '/' present -> match last component
};

// A compiled set of patterns sharing one base directory. `base` is the
// directory of the .gitignore file as an absolute path with no trailing slash.
class IgnoreSet {
public:
    IgnoreSet() = default;
    explicit IgnoreSet(std::string base) : base_(std::move(base)) {}

    // Parse ignore-file text and append its patterns to this set.
    void parse(std::string_view text);
    // Append a single raw gitignore pattern line.
    void add_pattern(std::string_view pattern);

    // Consult this set for a path expressed relative to base_.
    Match check(std::string_view rel_path, bool is_dir) const;

    const std::string& base() const { return base_; }
    bool empty() const { return patterns_.empty(); }
    std::size_t size() const { return patterns_.size(); }

private:
    std::string base_;
    std::vector<IgnorePattern> patterns_;

    static bool match_pattern(const IgnorePattern& p, std::string_view rel_path, bool is_dir);
};

// A stack of IgnoreSets ordered from outermost (root) to innermost (current),
// mirroring how git composes nested .gitignore files. The last (deepest, then
// latest-in-file) matching pattern decides; '!' negations re-include.
class IgnoreStack {
public:
    void push(IgnoreSet set) { sets_.push_back(std::move(set)); }
    void pop() { if (!sets_.empty()) sets_.pop_back(); }
    std::size_t size() const { return sets_.size(); }
    const std::vector<IgnoreSet>& sets() const { return sets_; }

    // `abs_path` is an absolute path with no trailing slash. `is_dir` marks a
    // directory candidate. Returns true if the path should be ignored.
    bool is_ignored(std::string_view abs_path, bool is_dir) const;

private:
    std::vector<IgnoreSet> sets_;
};

}  // namespace tokenc
