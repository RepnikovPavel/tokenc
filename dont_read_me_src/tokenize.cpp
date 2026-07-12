// SPDX-License-Identifier: 0BSD
#include "tokenize.hpp"

#include "fs.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace tokenc {
namespace {

std::string exe_dir()
{
    char buf[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
        return "";
    buf[n] = '\0';
    std::string path(buf);
    const auto slash = path.find_last_of('/');
    if (slash == std::string::npos)
        return "";
    return path.substr(0, slash);
}

std::string tokenize_script_path()
{
    if (const char* env = std::getenv("TOKENC_TOKENIZE_SCRIPT")) {
        if (env[0] != '\0' && fs::file_exists(env))
            return std::string(env);
    }

    const std::vector<std::string> candidates = {
        "/usr/local/share/tokenc/tokenc_tokenize.py",
        "/usr/share/tokenc/tokenc_tokenize.py",
        fs::join(exe_dir(), "tokenc_tokenize.py"),
        fs::join(exe_dir(), "../share/tokenc/tokenc_tokenize.py"),
        fs::join(exe_dir(), "../../scripts/tokenc_tokenize.py"),
        "scripts/tokenc_tokenize.py",
    };
    for (const auto& c : candidates) {
        if (fs::file_exists(c))
            return c;
    }
    return "";
}

bool run_python_tokenizer(const std::vector<std::string>& abs_paths,
                          std::unordered_map<std::string, TokenCounts>& out)
{
    const std::string script = tokenize_script_path();
    if (script.empty())
        return false;

    std::string list_path = fs::join(fs::cache_dir(), "tokenize_paths.txt");
    {
        std::ofstream f(list_path, std::ios::trunc);
        if (!f)
            return false;
        for (const auto& p : abs_paths)
            f << p << '\n';
    }

    const std::string out_path = list_path + ".out";
    std::string cmd = "python3 \"";
    cmd += script;
    cmd += "\" < \"";
    cmd += list_path;
    cmd += "\" > \"";
    cmd += out_path;
    cmd += "\" 2>/dev/null";

    if (std::system(cmd.c_str()) != 0)
        return false;

    std::ifstream in(out_path);
    if (!in)
        return false;

    std::string s;
    while (std::getline(in, s)) {
        if (s.empty())
            continue;

        const std::size_t t1 = s.find('\t');
        if (t1 == std::string::npos)
            continue;
        const std::size_t t2 = s.find('\t', t1 + 1);
        if (t2 == std::string::npos)
            continue;

        const std::string path = s.substr(0, t1);
        try {
            TokenCounts tc;
            tc.cl100k_base = std::stoull(s.substr(t1 + 1, t2 - t1 - 1));
            tc.o200k_base = std::stoull(s.substr(t2 + 1));
            out.emplace(path, tc);
        } catch (...) {
            continue;
        }
    }
    return !out.empty();
}

}  // namespace

bool tokenizers_available()
{
    if (tokenize_script_path().empty())
        return false;
    FILE* pipe = ::popen("python3 -c \"import tiktoken\" 2>/dev/null", "r");
    if (!pipe)
        return false;
    ::pclose(pipe);
    return true;
}

bool tokenize_files(const std::vector<std::string>& abs_paths,
                    std::unordered_map<std::string, TokenCounts>& out)
{
    out.clear();
    if (abs_paths.empty())
        return true;
    return run_python_tokenizer(abs_paths, out);
}

}  // namespace tokenc