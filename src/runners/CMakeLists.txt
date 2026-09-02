# Pinned, unmodified WDC65C02 core used only by software runners. Define the
# small native target here instead of importing qe6502's optional bindings,
# tools, install rules, and upstream test harnesses into Neo1.
add_library(neo1_qe6502 STATIC
    ${CMAKE_SOURCE_DIR}/lib/qe6502/cpu/src/qe6502.c
)

target_include_directories(neo1_qe6502 PUBLIC
    ${CMAKE_SOURCE_DIR}/lib/qe6502/cpu/include
)

target_compile_options(neo1_qe6502 PRIVATE
    -Wall
    -Wextra
    -Werror
)

# Software CPU execution shared by host-style Neo1 targets. Each runner owns
# its CPU state and services qe6502's explicit bus requests through its attached
# neo1_machine.
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

target_link_libraries(neo1_soft_runner PUBLIC neo1_machine neo1_qe6502)
