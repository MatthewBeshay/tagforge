# tagforge test executables. Source list grows phase by phase.

set(TAGFORGE_TEST_SOURCES
    unit/test_smoke.cpp
    # Foundations
    unit/test_format.cpp
    unit/test_tag_id.cpp
    unit/test_error.cpp
    unit/test_cursor.cpp
    unit/test_mutf8.cpp
    unit/test_value_accessors.cpp
    # Java skipper
    unit/test_skip_java.cpp
    # Java reader
    unit/test_decode_java.cpp
    # Java writer + round-trip
    unit/test_roundtrip_java.cpp
    # Zero-copy view
    unit/test_view_java.cpp
    # Bedrock
    unit/test_bedrock_le.cpp
    unit/test_bedrock_varint.cpp
    # SNBT
    unit/test_snbt.cpp
    # Compression
    unit/test_compress.cpp
    # Region
    unit/test_region.cpp
    # Completeness additions
    unit/test_auto_detect.cpp
    unit/test_io.cpp
    unit/test_lz4.cpp
    unit/test_view_bedrock.cpp
    unit/test_path.cpp
    unit/test_streaming_api.cpp
    unit/test_limits.cpp
)

add_executable(tagforge_tests ${TAGFORGE_TEST_SOURCES})

target_link_libraries(tagforge_tests
    PRIVATE
        tagforge::tagforge
        Catch2::Catch2WithMain
)

tagforge_set_compile_options(tagforge_tests)
tagforge_apply_sanitisers(tagforge_tests)

# Limit-tests deliberately recurse near the depth cap (512). Under ASAN each
# frame bloats with shadow-memory bookkeeping, so the default 1 MiB Windows
# stack overflows before the cap fires. 8 MiB leaves comfortable headroom.
if(MSVC)
    target_link_options(tagforge_tests PRIVATE /STACK:8388608)
endif()

target_compile_definitions(tagforge_tests PRIVATE
    TAGFORGE_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

catch_discover_tests(tagforge_tests)
