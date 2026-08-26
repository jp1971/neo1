# Software CPU execution shared by host-style Neo1 targets. The current
# fake65c02 dependency is process-global, so this runner supports one active
# instance while keeping CPU state outside neo1_machine_t.
add_library(neo1_soft_runner STATIC
    neo1_soft_runner.c
)

target_include_directories(neo1_soft_runner PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_options(neo1_soft_runner PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wno-unused-function
)

target_link_libraries(neo1_soft_runner PUBLIC neo1_machine)
