// SPDX-License-Identifier: 0BSD
#include "tokenize.hpp"

#include "fs.hpp"

#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "tokenize_script.inc"

namespace tokenc {
namespace {

constexpr const char* kScriptVersion = "tokenc-embed-version: 2";
constexpr const char* kScriptName = "tokenc_tokenize.py";

std::string python3_path()
{
    if (const char* env = std::getenv("TOKENC_PYTHON")) {
        if (env[0] != '\0' && fs::file_exists(env))
            return std::string(env);
    }
    const char* candidates[] = {"/usr/bin/python3", "/usr/local/bin/python3", "python3"};
    for (const char* c : candidates) {
        if (c[0] == '/' && !fs::file_exists(c))
            continue;
        std::string cmd = std::string(c) + " -c \"import sys\" 2>/dev/null";
        if (std::system(cmd.c_str()) == 0)
            return c[0] == '/' ? std::string(c) : "python3";
    }
    return "";
}

bool file_starts_with(const std::string& path, const char* prefix)
{
    std::string content;
    if (!fs::read_file(path, content))
        return false;
    return content.rfind(prefix, 0) == 0;
}

std::string materialize_embedded_script()
{
    const std::string dir = fs::cache_dir();
    const std::string path = fs::join(dir, kScriptName);
    if (file_starts_with(path, kScriptVersion))
        return path;

    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out)
            return "";
        out << kEmbeddedTokenizeScript;
        if (!out.good())
            return "";
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0)
        return "";
    return path;
}

std::string tokenize_script_path()
{
    if (const char* env = std::getenv("TOKENC_TOKENIZE_SCRIPT")) {
        if (env[0] != '\0' && fs::file_exists(env))
            return std::string(env);
    }

    const std::string embedded = materialize_embedded_script();
    if (!embedded.empty())
        return embedded;

    const std::vector<std::string> candidates = {
        "/usr/local/share/tokenc/tokenc_tokenize.py",
        "/usr/share/tokenc/tokenc_tokenize.py",
        "scripts/tokenc_tokenize.py",
    };
    for (const auto& c : candidates) {
        if (fs::file_exists(c))
            return c;
    }
    return "";
}

bool run_python_tokenizer(const std::vector<std::string>& abs_paths,
                          std::unordered_map<std::string, TokenCounts>& out,
                          std::string& err)
{
    err.clear();
    const std::string script = tokenize_script_path();
    const std::string python = python3_path();
    if (script.empty()) {
        err = "tokenize script not found";
        return false;
    }
    if (python.empty()) {
        err = "python3 not found";
        return false;
    }

    const std::string list_path = fs::join(fs::cache_dir(), "tokenize_paths.txt");
    const std::string out_path = list_path + ".out";
    const std::string err_path = list_path + ".err";

    {
        std::ofstream f(list_path, std::ios::trunc);
        if (!f) {
            err = "cannot write paths file";
            return false;
        }
        for (const auto& p : abs_paths)
            f << p << '\n';
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        err = "fork failed";
        return false;
    }
    if (pid == 0) {
        int errfd = ::open(err_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (errfd >= 0) {
            ::dup2(errfd, STDERR_FILENO);
            ::close(errfd);
        }
        ::execl(python.c_str(),
                python.c_str(),
                script.c_str(),
                "--paths-file",
                list_path.c_str(),
                "--output-file",
                out_path.c_str(),
                static_cast<char*>(nullptr));
        _exit(127);
    }

    int status = 0;
    if (::waitpid(pid, &status, 0) < 0) {
        err = "waitpid failed";
        return false;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::string msg;
        (void)fs::read_file(err_path, msg);
        if (msg.empty())
            err = "python tokenizer failed (pip install tiktoken?)";
        else
            err = msg;
        while (!err.empty() && (err.back() == '\n' || err.back() == '\r'))
            err.pop_back();
        return false;
    }

    std::ifstream in(out_path);
    if (!in) {
        err = "tokenizer produced no output";
        return false;
    }

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
    if (out.empty()) {
        err = "tokenizer output empty";
        return false;
    }
    return true;
}

}  // namespace

bool tokenizers_available()
{
    if (tokenize_script_path().empty() || python3_path().empty())
        return false;
    const std::string python = python3_path();
    std::string cmd = python + " -c \"import tiktoken\" 2>/dev/null";
    return std::system(cmd.c_str()) == 0;
}

bool tokenize_files(const std::vector<std::string>& abs_paths,
                    std::unordered_map<std::string, TokenCounts>& out,
                    std::string& err)
{
    out.clear();
    if (abs_paths.empty())
        return true;
    return run_python_tokenizer(abs_paths, out, err);
}

}  // namespace tokenc