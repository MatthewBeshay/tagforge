# Sanitiser wiring. Enabled by -DTAGFORGE_ENABLE_ASAN=ON (typically set by the
# msvc-asan / clang-cl-asan presets).

function(tagforge_apply_sanitisers target)
    if(NOT TAGFORGE_ENABLE_ASAN)
        return()
    endif()

    if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # MSVC's cl: ASan only. UBSan is not supported.
        target_compile_options(${target} PRIVATE /fsanitize=address /Zi)
        # Linker picks up the ASan runtime automatically when /fsanitize=address
        # is on each .obj. No /INFERASANLIBS in modern toolsets.
        # Disable STL container annotations: vcpkg-provided libraries (CLI11
        # etc.) are built without them, and mixing causes LNK2038 mismatches.
        target_compile_definitions(${target} PRIVATE
            _DISABLE_VECTOR_ANNOTATION
            _DISABLE_STRING_ANNOTATION
            _DISABLE_OPTIONAL_ANNOTATION
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # clang-cl: ASan + UBSan.
        target_compile_options(${target} PRIVATE
            /fsanitize=address
            -fsanitize=undefined
            -fno-omit-frame-pointer
            /Zi
        )
        target_link_options(${target} PRIVATE
            -fsanitize=address
            -fsanitize=undefined
        )
        target_compile_definitions(${target} PRIVATE
            _DISABLE_VECTOR_ANNOTATION
            _DISABLE_STRING_ANNOTATION
            _DISABLE_OPTIONAL_ANNOTATION
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
            -g
        )
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined
        )
    endif()
endfunction()
