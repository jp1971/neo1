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

add_executable(neo1_vaci_basic_tests
    test_vaci_basic.c
)

target_include_directories(neo1_vaci_basic_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ram
)

target_compile_options(neo1_vaci_basic_tests PRIVATE
    -Wall
    -Wextra
    -Werror
    # The included fake65c02 dependency carries one unused internal helper.
    -Wno-unused-function
)

add_test(NAME neo1_vaci_basic_round_trip COMMAND neo1_vaci_basic_tests)
