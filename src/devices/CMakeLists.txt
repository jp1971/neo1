# Shared Apple-1 keyboard/display register model. Runners provide input and
# consume display bytes; this library owns the 6502-visible PIA-like state.
add_library(neo1_apple1_pia STATIC
    neo1_apple1_pia.c
)

target_include_directories(neo1_apple1_pia PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_options(neo1_apple1_pia PRIVATE -Wall -Wextra)
