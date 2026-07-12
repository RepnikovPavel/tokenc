cmake_minimum_required(VERSION 3.10)

project(tokenc VERSION 1.1.0 LANGUAGES CXX)

# --- Build settings --------------------------------------------------------
# C++17 and g++ 9+ are deliberately the only compiler requirements, so tokenc
# builds on the oldest mainstream Linux distros (Ubuntu 20.04, Debian 11,
# RHEL 9) without a toolchain upgrade.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Release: optimize and drop unused sections for a smaller binary.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra)
    add_link_options($<$<CONFIG:RELEASE>:-Wl,--gc-sections>)
endif()

# --- The executable --------------------------------------------------------
add_executable(tokenc
    dont_read_me_src/main.cpp
    dont_read_me_src/fs.cpp
    dont_read_me_src/gitignore.cpp
    dont_read_me_src/walker.cpp
    dont_read_me_src/languages.cpp
    dont_read_me_src/counter.cpp
    dont_read_me_src/cache.cpp
    dont_read_me_src/output.cpp
    dont_read_me_src/tokenize.cpp
)

target_include_directories(tokenc PRIVATE dont_read_me_src)
find_package(Threads REQUIRED)
target_link_libraries(tokenc PRIVATE Threads::Threads)

# Statically link the C++ runtime so the binary runs on any glibc-based Linux
# without matching libstdc++.so versions. No runtime dependency beyond glibc.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_link_options(tokenc PRIVATE -static-libgcc -static-libstdc++)
endif()

install(TARGETS tokenc RUNTIME DESTINATION bin)
install(FILES scripts/tokenc_tokenize.py DESTINATION share/tokenc
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
