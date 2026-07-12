cmake_minimum_required(VERSION 3.10)

project(tokenc VERSION 1.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra)
    add_link_options($<$<CONFIG:RELEASE>:-Wl,--gc-sections>)
endif()

set(CL100K_VOCAB_Z "${CMAKE_CURRENT_BINARY_DIR}/cl100k_vocab.z")
set(CL100K_VOCAB_INC "${CMAKE_CURRENT_BINARY_DIR}/cl100k_vocab.inc")
add_custom_command(
    OUTPUT "${CL100K_VOCAB_Z}" "${CL100K_VOCAB_INC}"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/data/cl100k_vocab.z" "${CL100K_VOCAB_Z}"
    COMMAND xxd -i cl100k_vocab.z > "${CL100K_VOCAB_INC}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS "${CMAKE_SOURCE_DIR}/data/cl100k_vocab.z"
    COMMENT "Embedding cl100k_base vocabulary"
)

add_executable(tokenc
    dont_read_me_src/main.cpp
    dont_read_me_src/fs.cpp
    dont_read_me_src/gitignore.cpp
    dont_read_me_src/walker.cpp
    dont_read_me_src/languages.cpp
    dont_read_me_src/counter.cpp
    dont_read_me_src/cache.cpp
    dont_read_me_src/output.cpp
    dont_read_me_src/cl100k.cpp
    dont_read_me_src/tokenize.cpp
    "${CL100K_VOCAB_INC}"
)

target_include_directories(tokenc PRIVATE dont_read_me_src ${CMAKE_CURRENT_BINARY_DIR})

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PCRE2 REQUIRED libpcre2-8)
pkg_check_modules(ZLIB REQUIRED zlib)

target_link_libraries(tokenc PRIVATE Threads::Threads ${PCRE2_LIBRARIES} ${ZLIB_LIBRARIES})
target_include_directories(tokenc PRIVATE ${PCRE2_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_link_options(tokenc PRIVATE -static-libgcc -static-libstdc++)
endif()

install(TARGETS tokenc RUNTIME DESTINATION bin)