add_executable(neo1_msc_tests
    test_neo1_msc.c
    ${CMAKE_SOURCE_DIR}/systems/neo1-pico/src/neo1_msc.c
)

# The test-only ff.h and fake FatFs implementation let this target execute the
# production Pico register state machine against deterministic disposable data.
target_include_directories(neo1_msc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fake_fatfs
    ${CMAKE_SOURCE_DIR}/systems/neo1-pico/src
)

target_compile_definitions(neo1_msc_tests PRIVATE
    NEO1_MSC_DEBUG=0
)

target_compile_options(neo1_msc_tests PRIVATE -Wall -Wextra -Werror)

add_test(NAME neo1_msc_register_contract COMMAND neo1_msc_tests)

add_executable(neo1_vaci_payload_tests
    test_vaci_basic.c
)

target_include_directories(neo1_vaci_payload_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ram
)

target_compile_options(neo1_vaci_payload_tests PRIVATE
    -Wall
    -Wextra
    -Werror
    # The included fake65c02 dependency carries one unused internal helper.
    -Wno-unused-function
)

add_test(NAME neo1_vaci_payload_contract COMMAND neo1_vaci_payload_tests)

foreach(msc_enabled IN ITEMS 0 1)
    set(msc_decode_target neo1_msc_decode_${msc_enabled}_tests)
    add_executable(${msc_decode_target}
        test_neo1_msc_decode.c
    )
    target_include_directories(${msc_decode_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/systems/neo1-pico/src
    )
    target_compile_definitions(${msc_decode_target} PRIVATE
        NEO1_CPU_BACKEND=3
        NEO1_ROM_BASE=0xFF00
        NEO1_ROM_PROTECT_BASE=0xFF00
        NEO1_ENABLE_MSC=${msc_enabled}
        NEO1_ENABLE_VCFFA1=0
        NEO1_DIAGNOSTICS=0
    )
    target_compile_options(${msc_decode_target} PRIVATE
        -Wall
        -Wextra
        -Werror
        -Wno-unused-function
    )
endforeach()

add_test(NAME neo1_msc_decode_disabled_uses_ram COMMAND neo1_msc_decode_0_tests)
add_test(NAME neo1_msc_decode_enabled_uses_device COMMAND neo1_msc_decode_1_tests)

add_executable(neo1_soft_timing_tests
    test_neo1_soft_timing.c
)
target_include_directories(neo1_soft_timing_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(neo1_soft_timing_tests PRIVATE
    NEO1_CPU_BACKEND=3
    NEO1_ROM_BASE=0xFF00
    NEO1_ROM_PROTECT_BASE=0xFF00
    NEO1_ENABLE_MSC=0
    NEO1_ENABLE_VCFFA1=0
    NEO1_DIAGNOSTICS=0
)
target_compile_options(neo1_soft_timing_tests PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wno-unused-function
)

add_test(NAME neo1_soft_cycle_budget COMMAND neo1_soft_timing_tests)

add_executable(neo1_terminal_tests
    test_neo1_terminal.c
    ${CMAKE_SOURCE_DIR}/systems/neo1-pico/src/neo1_terminal_pico.c
    ${CMAKE_SOURCE_DIR}/systems/neo1-sdl/src/neo1_terminal_sdl.c
)
target_include_directories(neo1_terminal_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/systems/neo1-pico/src
    ${CMAKE_SOURCE_DIR}/systems/neo1-sdl/src
)
target_compile_options(neo1_terminal_tests PRIVATE
    -Wall
    -Wextra
    -Werror
)
target_link_libraries(neo1_terminal_tests PRIVATE neo1_terminal_grid)

add_test(NAME neo1_terminal_grid_contract COMMAND neo1_terminal_tests)
