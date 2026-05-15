cmake_minimum_required(VERSION 4.0)

project(tagforge
    VERSION 1.0.0
    DESCRIPTION "A modern C++23 NBT library for Minecraft (Java + Bedrock)."
    HOMEPAGE_URL "https://github.com/tagforge/tagforge"
    LANGUAGES C CXX
)

# ---------------------------------------------------------------------------
# Project-wide options
# ---------------------------------------------------------------------------

option(TAGFORGE_BUILD_TESTING       "Build tagforge unit + property tests." ON)
option(TAGFORGE_BUILD_CLI           "Build the nbt-cli debugging tool." OFF)
option(TAGFORGE_BUILD_BENCHMARKS    "Build tagforge in-repo self-benchmarks (requires nanobench)." OFF)
option(TAGFORGE_ENABLE_ASAN         "Enable AddressSanitizer (and UBSan on Clang)." OFF)
option(TAGFORGE_WARNINGS_AS_ERRORS  "Treat warnings as errors." OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(TagforgeCompileOptions)
include(TagforgeSanitisers)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

find_package(libdeflate CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Public header interface library
# ---------------------------------------------------------------------------

add_library(tagforge_headers INTERFACE)
add_library(tagforge::tagforge_headers ALIAS tagforge_headers)
target_include_directories(tagforge_headers INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(tagforge_headers INTERFACE cxx_std_23)

set(TAGFORGE_SOURCES
    src/compress/detect.cpp
    src/compress/libdeflate_wrap.cpp
    src/compress/lz4_wrap.cpp
    src/decode/decode_bedrock_le.cpp
    src/decode/decode_bedrock_varint.cpp
    src/decode/decode_dispatch.cpp
    src/decode/decode_java.cpp
    src/encode/encode_bedrock_le.cpp
    src/encode/encode_bedrock_varint.cpp
    src/encode/encode_dispatch.cpp
    src/encode/encode_java.cpp
    src/region/region_file.cpp
    src/skip/skip_bedrock_le.cpp
    src/skip/skip_bedrock_varint.cpp
    src/skip/skip_dispatch.cpp
    src/skip/skip_java.cpp
    src/snbt/snbt_lexer.cpp
    src/snbt/snbt_parser.cpp
    src/snbt/snbt_writer.cpp
    src/view/view_bedrock_le.cpp
    src/view/view.cpp
    src/view/view_java.cpp
    src/view/view_bedrock_varint.cpp
    src/cursor.cpp
    src/error.cpp
    src/format.cpp
    src/io.cpp
    src/mutf8.cpp
    src/path.cpp
    src/value.cpp
)

add_library(tagforge STATIC ${TAGFORGE_SOURCES})
add_library(tagforge::tagforge ALIAS tagforge)

target_link_libraries(tagforge
    PUBLIC  tagforge::tagforge_headers
    PRIVATE
        $<IF:$<TARGET_EXISTS:libdeflate::libdeflate_shared>,libdeflate::libdeflate_shared,libdeflate::libdeflate_static>
        lz4::lz4
)

tagforge_set_compile_options(tagforge)
tagforge_apply_sanitisers(tagforge)

if(TAGFORGE_WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(tagforge PRIVATE /WX)
    else()
        target_compile_options(tagforge PRIVATE -Werror)
    endif()
endif()

if(TAGFORGE_BUILD_TESTING)
    enable_testing()
    find_package(Catch2 3 CONFIG REQUIRED)
    include(Catch)
    add_subdirectory(tests)
endif()

if(TAGFORGE_BUILD_CLI)
    add_subdirectory(tools/nbt-cli)
endif()

if(TAGFORGE_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(PROJECT_IS_TOP_LEVEL)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS tagforge tagforge_headers
    EXPORT tagforgeTargets
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/tagforge
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT tagforgeTargets
    FILE tagforgeTargets.cmake
    NAMESPACE tagforge::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tagforge
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/tagforge-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/tagforge-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tagforge
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/tagforge-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/tagforge-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/tagforge-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tagforge
)
endif() # PROJECT_IS_TOP_LEVEL
