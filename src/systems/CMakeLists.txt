# CPU-neutral 6502-visible Neo1 machine state. CPU runners supply bus accesses;
# this library owns RAM, ROM policy, PIA state, and optional-device decode.
add_library(neo1_machine STATIC
    neo1_machine.c
)

target_include_directories(neo1_machine PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_options(neo1_machine PRIVATE -Wall -Wextra)
target_link_libraries(neo1_machine PUBLIC neo1_apple1_pia)
