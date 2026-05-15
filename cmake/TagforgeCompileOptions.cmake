# Per-target compile options for tagforge.
#
# All warnings are project-internal: applied only to targets we own (the library,
# tests, benchmarks, CLI). Third-party vcpkg dependencies are linked through
# imported targets and are not affected.

function(tagforge_set_compile_options target)
    target_compile_features(${target} PUBLIC cxx_std_23)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    if(MSVC)
        # /W4 + permissive- on both cl and clang-cl. /Zc:preprocessor on for the
        # conforming C++ preprocessor; /utf-8 so string literals match the
        # encoding the .clang-format file enforces.
        #
        # /w14242 - conversion from larger to smaller type (off-by-default at /W4).
        # /w14254 - conversion of bitfield to wider type.
        # /w14263 - member function does not override any base class virtual
        #           member function (subtle override-mismatch warning).
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            # /Zc:preprocessor is only meaningful for cl.exe; clang-cl uses a
            # conforming preprocessor unconditionally and warns about the flag.
            $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>
            /Zc:__cplusplus
            /utf-8
            /EHsc
            /w14242 /w14254 /w14263
            $<$<CONFIG:Debug>:/Od>
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:RelWithDebInfo>:/O2>
        )
        target_compile_definitions(${target} PRIVATE
            _CRT_SECURE_NO_WARNINGS
            NOMINMAX
            WIN32_LEAN_AND_MEAN
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wconversion -Wshadow -Wnon-virtual-dtor
            -Wold-style-cast -Wcast-align -Wunused
            -Woverloaded-virtual -Wdouble-promotion
            -Wformat=2
        )
    endif()
endfunction()
