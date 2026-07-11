// SPDX-License-Identifier: 0BSD
#include "languages.hpp"

#include <array>
#include <cctype>
#include <string>
#include <unordered_map>
#include <utility>

namespace tokenc {
namespace {

// Lowercase an ASCII string in place.
std::string to_lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Extract the last extension (including the dot) from a path. Returns "" for
// dotfiles with no real extension (e.g. ".gitignore") and for files without
// any dot. "foo.tar.gz" -> ".gz" (matches typical language-detection tools).
std::string get_extension(std::string_view path)
{
    // Find the last path separator so we don't treat dots in directory names.
    const auto slash = path.find_last_of("/\\");
    const std::size_t name_start = (slash == std::string_view::npos) ? 0 : slash + 1;
    const std::string_view name = path.substr(name_start);

    // A leading dot (hidden file like ".gitignore") is not an extension.
    if (name.empty() || name[0] == '.')
        return "";

    const auto dot = name.rfind('.');
    if (dot == std::string_view::npos)
        return "";
    return to_lower(std::string(name.substr(dot)));
}

// Extension -> language display name. Built once into an unordered_map for
// O(1) lookups. Keep the list curated to genuine source languages only: data
// formats (json/yaml/xml/toml), markup (md/html) and config are deliberately
// absent so they are never counted as code.
const std::unordered_map<std::string, std::string_view>& extension_map()
{
    static const std::unordered_map<std::string, std::string_view> m = [] {
        std::unordered_map<std::string, std::string_view> m;
        // Pairs of {extension, language}. Sorted by language for readability.
        constexpr std::array<std::pair<std::string_view, std::string_view>, 200> entries = {{
            // Assembly
            {".asm", "Assembly"}, {".s", "Assembly"}, {".S", "Assembly"},
            // C / C++
            {".c", "C"}, {".h", "C/C++ Header"},
            {".cc", "C++"}, {".cpp", "C++"}, {".cxx", "C++"}, {".c++", "C++"},
            {".hpp", "C++ Header"}, {".hh", "C++ Header"}, {".hxx", "C++ Header"}, {".h++", "C++ Header"},
            {".inl", "C++ Header"},
            // C#
            {".cs", "C#"},
            // Clojure
            {".clj", "Clojure"}, {".cljs", "ClojureScript"}, {".cljc", "Clojure"}, {".edn", "Clojure"},
            // D
            {".d", "D"}, {".di", "D"},
            // Dart
            {".dart", "Dart"},
            // Delphi / Pascal
            {".pas", "Pascal"}, {".pp", "Pascal"}, {".inc", "Pascal"},
            // Elixir
            {".ex", "Elixir"}, {".exs", "Elixir"},
            // Erlang
            {".erl", "Erlang"}, {".hrl", "Erlang"},
            // Fortran
            {".f", "Fortran"}, {".for", "Fortran"}, {".f90", "Fortran"}, {".f95", "Fortran"},
            {".f03", "Fortran"}, {".f08", "Fortran"},
            // Go
            {".go", "Go"},
            // Groovy
            {".groovy", "Groovy"}, {".gradle", "Groovy"},
            // Haskell
            {".hs", "Haskell"}, {".lhs", "Haskell"},
            // Java
            {".java", "Java"},
            // JavaScript / TypeScript
            {".js", "JavaScript"}, {".mjs", "JavaScript"}, {".cjs", "JavaScript"},
            {".jsx", "JavaScript"}, {".ts", "TypeScript"}, {".tsx", "TypeScript"}, {".mts", "TypeScript"}, {".cts", "TypeScript"},
            // Julia
            {".jl", "Julia"},
            // Kotlin
            {".kt", "Kotlin"}, {".kts", "Kotlin"},
            // Lisp
            {".lisp", "Lisp"}, {".lsp", "Lisp"}, {".cl", "Common Lisp"}, {".el", "Emacs Lisp"}, {".scm", "Scheme"}, {".ss", "Scheme"},
            // Lua
            {".lua", "Lua"},
            // Nim
            {".nim", "Nim"}, {".nims", "Nim"},
            // Objective-C
            {".m", "Objective-C"}, {".mm", "Objective-C++"},
            // OCaml
            {".ml", "OCaml"}, {".mli", "OCaml"},
            // Perl
            {".pl", "Perl"}, {".pm", "Perl"}, {".t", "Perl"}, {".pod", "Perl"},
            // PHP
            {".php", "PHP"}, {".phtml", "PHP"}, {".php3", "PHP"}, {".php4", "PHP"}, {".php5", "PHP"},
            // PowerShell
            {".ps1", "PowerShell"}, {".psm1", "PowerShell"}, {".psd1", "PowerShell"},
            // Proto / Thrift / IDL
            {".proto", "Protocol Buffers"}, {".thrift", "Thrift"},
            // Python
            {".py", "Python"}, {".pyi", "Python"}, {".pyw", "Python"},
            // R
            {".r", "R"}, {".R", "R"},
            // Ruby
            {".rb", "Ruby"}, {".rbi", "Ruby"}, {".rake", "Ruby"}, {".gemspec", "Ruby"},
            // Rust
            {".rs", "Rust"},
            // Scala
            {".scala", "Scala"}, {".sc", "Scala"},
            // Shell
            {".sh", "Shell"}, {".bash", "Shell"}, {".zsh", "Shell"}, {".fish", "Shell"},
            {".ksh", "Shell"}, {".csh", "Shell"},
            // SQL
            {".sql", "SQL"}, {".psql", "SQL"}, {".plsql", "SQL"},
            // Swift
            {".swift", "Swift"},
            // Vim
            {".vim", "Vim Script"},
            // Vue / Svelte (component frameworks; code-bearing)
            {".vue", "Vue"}, {".svelte", "Svelte"},
            // Zig
            {".zig", "Zig"},
            // Make / CMake
            {".mk", "Make"}, {".mak", "Make"}, {".make", "Make"},
            {"GNUmakefile", "Make"}, {"Makefile", "Make"}, {"makefile", "Make"},
            {".cmake", "CMake"},
            {"CMakeLists.txt", "CMake"},
            // Misc well-known source filenames without extensions
            {"Dockerfile", "Dockerfile"}, {"Containerfile", "Dockerfile"},
            {"Rakefile", "Ruby"}, {"Vagrantfile", "Ruby"}, {"Gemfile", "Ruby"},
        }};
        for (const auto& [ext, lang] : entries)
            m.emplace(to_lower(std::string(ext)), lang);
        return m;
    }();
    return m;
}

}  // namespace

std::optional<std::string_view> language_for_path(std::string_view path)
{
    // First try a full-filename match (Makefile, Dockerfile, CMakeLists.txt...).
    const auto slash = path.find_last_of("/\\");
    const std::size_t name_start = (slash == std::string_view::npos) ? 0 : slash + 1;
    const std::string name(to_lower(std::string(path.substr(name_start))));
    if (!name.empty()) {
        const auto& fm = extension_map();
        if (auto it = fm.find(name); it != fm.end())
            return it->second;
    }
    const std::string ext = get_extension(path);
    if (ext.empty())
        return std::nullopt;
    const auto& fm = extension_map();
    if (auto it = fm.find(ext); it != fm.end())
        return it->second;
    return std::nullopt;
}

const std::vector<std::string>& default_ignore_patterns()
{
    // Ordered, curated list. Grouped by concern. These represent "obvious"
    // non-code that a developer never wants counted.
    static const std::vector<std::string> patterns = [] {
        std::vector<std::string> v;
        // Version control
        for (const char* p : {
            ".git", ".svn", ".hg", ".bzr", ".gitignore", ".gitattributes", ".gitmodules",
            ".git-blame-ignore-revs", ".mailmap",
        }) v.emplace_back(p);
        // Dependency directories (vendored third-party code, package managers)
        for (const char* p : {
            "node_modules", "bower_components", "jspm_packages", ".pnp", ".yarn",
            "vendor", "vendors", "vendored",
            "third_party", "3rd-party", "3rdparty", "thirdparty",
            "external", "externals", "ext",
            "subprojects", "submodule", "submodules",
            "deps", "Dependencies", "dependencies",
            "Pods", "Carthage", "SwiftPackageManager", ".build",
            ".bundle", "gems",
        }) v.emplace_back(p);
        // Python caches / venvs. We catch venvs two ways: by name here, and by
        // the pyvenv.cfg / conda-meta markers in the walker (see walker.cpp),
        // which covers virtualenvs with arbitrary names.
        for (const char* p : {
            "__pycache__", ".pytest_cache", ".mypy_cache", ".ruff_cache", ".tox",
            ".nox", ".venv", ".venvs", "venv", "venvs", "env", "envs", ".env",
            ".eggs", "*.egg-info", "*.egg-info/", "*.egg-link",
            "site-packages", "dist-packages", ".python-version",
        }) v.emplace_back(p);
        // Build / output directories
        for (const char* p : {
            "build", "Build", "bin", "Bin", "obj", "Obj", "out", "Out",
            "dist", "Dist", "release", "Release", "debug", "Debug",
            "target", "cmake-build-debug", "cmake-build-release",
        }) v.emplace_back(p);
        // CMake artifacts
        for (const char* p : {
            "CMakeFiles", "CMakeCache.txt", "cmake_install.cmake",
            "install_manifest.txt", "compile_commands.json",
        }) v.emplace_back(p);
        // Java / JVM
        for (const char* p : {
            ".gradle", ".gradle-cache", "gradle-app.setting",
            ".classpath", ".project", ".settings", "bin",
            "*.class", "*.jar", "*.war", "*.ear", "*.nar",
        }) v.emplace_back(p);
        // Rust
        for (const char* p : {"target", "*.rlib"}) v.emplace_back(p);
        // Go
        for (const char* p : {"vendor", "*.test"}) v.emplace_back(p);
        // Ruby
        for (const char* p : {".bundle", "*.gem", "Gemfile.lock", "gems.locked"}) v.emplace_back(p);
        // .NET / NuGet
        for (const char* p : {"bin", "obj", "*.dll", "*.pdb", "*.exe", "*.cache", "packages"}) v.emplace_back(p);
        // Compiled object files / binaries / libraries
        for (const char* p : {
            "*.o", "*.obj", "*.a", "*.so", "*.so.*", "*.dylib", "*.dll",
            "*.lib", "*.exe", "*.wasm", "*.pdb",
        }) v.emplace_back(p);
        // Archives / packages / installers (not source; never hand-written code)
        for (const char* p : {
            "*.zip", "*.tar", "*.tar.gz", "*.tgz", "*.tar.bz2", "*.tbz2", "*.tar.xz", "*.txz",
            "*.gz", "*.bz2", "*.xz", "*.zst", "*.lz4", "*.7z", "*.rar",
            "*.jar", "*.war", "*.ear", "*.whl", "*.egg", "*.gem", "*.nupkg", "*.snupkg",
            "*.deb", "*.rpm", "*.apk", "*.msi", "*.pkg", "*.dmg", "*.iso",
            "*.vsix", "*.vsixmanifest",            // VS Code extension packages
            "*.crx", "*.xpi", "*.ova", "*.img",
        }) v.emplace_back(p);
        // Minified bundles & source maps (machine-generated, not hand-written code)
        for (const char* p : {"*.min.js", "*.min.mjs", "*.min.css", "*.map"}) v.emplace_back(p);
        // Note: generated-code filenames (*.pb.go, *_pb2.py, *.pb.cc, ...) are NOT
        // listed here. They are detected in the counter (see is_generated_file),
        // which is governed by the --include-generated flag. Putting them in this
        // default-ignore list would make them un-includable.
        // Lock files
        for (const char* p : {
            "package-lock.json", "yarn.lock", "pnpm-lock.yaml",
            "poetry.lock", "Pipfile.lock", "Cargo.lock", "composer.lock",
            "go.sum", "mix.lock", "uv.lock", "bun.lockb",
        }) v.emplace_back(p);
        // Coverage / test reports
        for (const char* p : {"coverage", ".nyc_output", "*.lcov", ".coverage"}) v.emplace_back(p);
        // IDE / editor
        for (const char* p : {
            ".idea", ".vscode", ".vs", ".fleet", ".history", ".stackedit",
        }) v.emplace_back(p);
        // OS metadata
        for (const char* p : {".DS_Store", "Thumbs.db", "desktop.ini"}) v.emplace_back(p);
        // Documentation generators' transient output
        for (const char* p : {".docusaurus", ".next", ".nuxt", ".svelte-kit", ".turbo", ".parcel-cache"}) v.emplace_back(p);
        return v;
    }();
    return patterns;
}

}  // namespace tokenc
