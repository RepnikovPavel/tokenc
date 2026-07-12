// SPDX-License-Identifier: 0BSD
#include "tokenize.hpp"

#include "cl100k.hpp"
#include "fs.hpp"

#include <fstream>
#include <string>

namespace tokenc {

bool tokenizers_available()
{
    return true;
}

bool tokenize_files(const std::vector<std::string>& abs_paths,
                    std::unordered_map<std::string, TokenCounts>& out,
                    std::string& err)
{
    out.clear();
    err.clear();
    if (abs_paths.empty())
        return true;

    const Cl100kEncoder& enc = Cl100kEncoder::instance();
    std::string data;

    for (const auto& path : abs_paths) {
        if (!fs::read_file(path, data)) {
            out.emplace(path, TokenCounts{});
            continue;
        }
        TokenCounts tc;
        tc.cl100k_base = enc.count_tokens(data);
        tc.o200k_base = 0;
        out.emplace(path, tc);
    }
    return true;
}

}  // namespace tokenc