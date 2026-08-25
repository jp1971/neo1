# Shared host-side 40x24 terminal grid. Pixel rendering and control-byte policy
# remain in each target; this library owns only state and primitive mutations.
add_library(neo1_terminal_grid STATIC
    neo1_terminal.c
)

target_include_directories(neo1_terminal_grid PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_options(neo1_terminal_grid PRIVATE -Wall -Wextra)
