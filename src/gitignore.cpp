// SPDX-License-Identifier: MIT
#include "gitignore.hpp"

#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace tokenc {
namespace {

// Split "a/b/c" into components ["a","b","c"]. Empty input -> empty vector.
std::vector<std::string_view> split_components(std::string_view path)
{
    std::vector<std::string_view> out;
    std::size_t pos = 0;
    while (pos < path.size()) {
        const auto slash = path.find('/', pos);
        if (slash == std::string_view::npos) {
            out.push_back(path.substr(pos));
            break;
        }
        out.push_back(path.substr(pos, slash - pos));
        pos = slash + 1;
    }
    return out;
}

// fnmatch-style match for a SINGLE path component (no '/' inside). '*' and '?'
// do not cross '/'. Supports '[...]' classes with ranges and leading '!'.
bool component_matches(std::string_view pat, std::string_view text)
{
    const auto* p = pat.data();
    const auto* p_end = p + pat.size();
    const auto* t = text.data();
    const auto* t_end = t + text.size();

    auto match = [&](auto&& self, const char* pp, const char* tt) -> bool {
        while (pp < p_end) {
            const char pc = *pp++;
            if (pc == '*') {
                while (pp < p_end && *pp == '*')
                    ++pp;
                if (pp == p_end)
                    return true;  // trailing '*' eats the rest of the component
                for (const char* tp = tt; ; ++tp) {
                    if (self(self, pp, tp))
                        return true;
                    if (tp == t_end)
                        return false;
                }
            } else if (pc == '?') {
                if (tt == t_end)
                    return false;
                ++tt;
            } else if (pc == '[') {
                if (tt == t_end)
                    return false;
                bool neg = false;
                if (pp < p_end && (*pp == '!' || *pp == '^')) {
                    neg = true;
                    ++pp;
                }
                bool matched = false;
                if (pp < p_end && *pp == ']') {  // leading ']' is literal
                    matched = (*tt == ']');
                    ++pp;
                }
                while (pp < p_end && *pp != ']') {
                    if (pp + 2 < p_end && pp[1] == '-' && pp[2] != ']') {
                        if (*tt >= pp[0] && *tt <= pp[2])
                            matched = true;
                        pp += 3;
                    } else {
                        if (*pp == *tt)
                            matched = true;
                        ++pp;
                    }
                }
                if (pp < p_end && *pp == ']')
                    ++pp;
                if (matched == neg)
                    return false;
                ++tt;
            } else {
                if (tt == t_end || *tt != pc)
                    return false;
                ++tt;
            }
        }
        return tt == t_end;
    };
    return match(match, p, t);
};

// Match a full relative path against a path pattern (one that contains '/').
// Components are split on '/'. "**" matches zero or more path components.
// `pat_comps` and `path_comps` are non-empty (caller ensured pattern has '/').
bool path_matches_comps(const std::vector<std::string_view>& pat_comps,
                        const std::vector<std::string_view>& path_comps)
{
    const std::size_t m = pat_comps.size();
    const std::size_t n = path_comps.size();

    // match(pi, ti): does pat_comps[pi:] match path_comps[ti:]?
    auto match = [&](auto&& self, std::size_t pi, std::size_t ti) -> bool {
        while (pi < m) {
            if (pat_comps[pi] == "**") {
                // "**" consumes zero or more components. Skip consecutive "**".
                do { ++pi; } while (pi < m && pat_comps[pi] == "**");
                if (pi == m)
                    return true;  // trailing "**" matches everything left
                for (std::size_t k = ti; k <= n; ++k)
                    if (self(self, pi, k))
                        return true;
                return false;
            }
            if (ti >= n)
                return false;
            if (!component_matches(pat_comps[pi], path_comps[ti]))
                return false;
            ++pi;
            ++ti;
        }
        return ti == n;
    };
    return match(match, 0, 0);
}

bool path_matches(std::string_view pattern, std::string_view path)
{
    return path_matches_comps(split_components(pattern), split_components(path));
}

}  // namespace

void IgnoreSet::add_pattern(std::string_view line)
{
    std::string_view s = line;
    // Trim trailing CR; git also trims surrounding spaces unless escaped.
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n'))
        s.remove_suffix(1);

    if (s.empty() || s[0] == '#')
        return;

    IgnorePattern pat;
    if (s[0] == '!') {
        pat.negate = true;
        s.remove_prefix(1);
    }
    // Unescape a leading backslash before special chars.
    if (s.size() >= 2 && s[0] == '\\' &&
        (s[1] == '!' || s[1] == '#' || s[1] == '[' || s[1] == ' ')) {
        s.remove_prefix(1);
    }

    // Trailing '/' -> directory-only; strip trailing slashes.
    while (!s.empty() && s.back() == '/') {
        pat.dir_only = true;
        s.remove_suffix(1);
    }

    // Leading '/' -> anchored to base.
    if (!s.empty() && s[0] == '/') {
        pat.anchored = true;
        s.remove_prefix(1);
    }

    if (s.empty())
        return;

    // A pattern with no '/' matches the basename at any depth.
    pat.match_basename = (s.find('/') == std::string_view::npos);
    pat.raw.assign(s);
    patterns_.push_back(std::move(pat));
}

void IgnoreSet::parse(std::string_view text)
{
    std::size_t pos = 0;
    while (pos < text.size()) {
        const auto nl = text.find('\n', pos);
        std::string_view line = (nl == std::string_view::npos)
                                    ? text.substr(pos)
                                    : text.substr(pos, nl - pos);
        add_pattern(line);
        if (nl == std::string_view::npos)
            break;
        pos = nl + 1;
    }
}

Match IgnoreSet::check(std::string_view rel_path, bool is_dir) const
{
    Match result = Match::None;
    for (const auto& p : patterns_) {
        if (match_pattern(p, rel_path, is_dir)) {
            result = p.negate ? Match::Unignored : Match::Ignored;
        }
    }
    return result;
}

bool IgnoreSet::match_pattern(const IgnorePattern& p, std::string_view rel_path, bool is_dir)
{
    if (p.dir_only && !is_dir)
        return false;

    if (p.match_basename) {
        const auto slash = rel_path.rfind('/');
        const std::string_view name = (slash == std::string_view::npos)
                                          ? rel_path
                                          : rel_path.substr(slash + 1);
        return component_matches(p.raw, name);
    }

    // A pattern containing '/' is anchored to the base directory of this set.
    // (A leading '/' only adds the anchoring semantics to what would otherwise
    // be a basename pattern; here it was already stripped, so behavior is the
    // same as a "middle-slash" pattern.)
    return path_matches(p.raw, rel_path);
}

bool IgnoreStack::is_ignored(std::string_view abs_path, bool is_dir) const
{
    // Walk from innermost (most specific) set to outermost. The first set that
    // returns a definite Ignored/Unignored decision wins, because more deeply
    // nested .gitignore files override ancestors. Within a set, the last
    // matching pattern already won (see IgnoreSet::check).
    for (auto it = sets_.rbegin(); it != sets_.rend(); ++it) {
        const IgnoreSet& set = *it;
        if (set.empty())
            continue;
        const std::string_view base = set.base();
        std::string_view rel;
        if (base.empty()) {
            rel = abs_path;
        } else if (abs_path.size() > base.size() + 1 &&
                   abs_path.compare(0, base.size(), base) == 0 &&
                   abs_path[base.size()] == '/') {
            rel = abs_path.substr(base.size() + 1);
        } else if (abs_path == base) {
            continue;  // base dir itself: never ignored by its own set
        } else {
            continue;  // path not under this set's base
        }
        const Match m = set.check(rel, is_dir);
        if (m == Match::Ignored)
            return true;
        if (m == Match::Unignored)
            return false;
        // Match::None: fall through to the next (more general) set.
    }
    return false;
}

}  // namespace tokenc
