find_package(CLI11 CONFIG REQUIRED)

add_executable(nbt-cli
    main.cpp
)

target_link_libraries(nbt-cli
    PRIVATE
        tagforge::tagforge
        CLI11::CLI11
)

tagforge_set_compile_options(nbt-cli)
tagforge_apply_sanitisers(nbt-cli)

set_target_properties(nbt-cli PROPERTIES OUTPUT_NAME "nbt-cli")
