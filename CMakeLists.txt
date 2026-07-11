cmake_minimum_required(VERSION 3.16)

project(tokenc
    VERSION 1.0.0
    DESCRIPTION "Fast line counter for source code, with .gitignore awareness"
    LANGUAGES CXX)

# --- Build settings ----------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Aggressive optimization for a release line-counter.
foreach(flag_var CMAKE_CXX_FLAGS_RELEASE)
    if(NOT ${flag_var} MATCHES "-ffunction-sections")
        string(APPEND ${flag_var} " -ffunction-sections -fdata-sections")
    endif()
endforeach()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_link_options($<$<CONFIG:RELEASE>:-Wl,--gc-sections>)
endif()

option(TOKENC_STATIC_STDLIB "Link the C++ runtime statically for a portable binary" ON)

# --- The executable ----------------------------------------------------------
add_executable(tokenc
    src/main.cpp
    src/fs.cpp
    src/gitignore.cpp
    src/walker.cpp
    src/languages.cpp
    src/counter.cpp
    src/cache.cpp
    src/output.cpp
)

target_include_directories(tokenc PRIVATE src)

target_compile_features(tokenc PRIVATE cxx_std_20)

# POSIX: pthread for std::thread on some toolchains.
find_package(Threads REQUIRED)
target_link_libraries(tokenc PRIVATE Threads::Threads)

# Optional static linking of the C++ standard library for a self-contained
# binary that has no runtime dependency on libstdc++.so version mismatches.
if(TOKENC_STATIC_STDLIB)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(tokenc PRIVATE -static-libgcc -static-libstdc++)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_link_options(tokenc PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()

# --- Install -----------------------------------------------------------------
install(TARGETS tokenc RUNTIME DESTINATION bin)
