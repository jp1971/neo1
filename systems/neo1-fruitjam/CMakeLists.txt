# RP2350 software-CPU runner with Fruit Jam-owned HSTX DVI. USB-CDC supplies
# temporary console input; this checkpoint intentionally links no USB-host
# keyboard, audio, storage, VACI, VCFFA1, PicoDVI, or physical-W65C02 services.
set(NEO1_PERSONALITY "50" CACHE STRING "Neo1 personality: 23 or 50")
set_property(CACHE NEO1_PERSONALITY PROPERTY STRINGS 23 50)

if (NOT NEO1_PERSONALITY MATCHES "^(23|50)$")
    message(FATAL_ERROR "NEO1_PERSONALITY must be 23 or 50")
endif()

foreach(feature IN ITEMS NEO1_ENABLE_VACI NEO1_ENABLE_MSC NEO1_ENABLE_VCFFA1)
    if (NOT DEFINED ${feature})
        set(${feature} "0" CACHE STRING "Disabled by the current Fruit Jam target")
    endif()
    if (NOT ${feature} STREQUAL "0")
        message(FATAL_ERROR "${feature} must remain 0 for the current Fruit Jam target")
    endif()
endforeach()

add_executable(neo1
    src/main.c
    src/neo1_hstx_video.c
)

target_compile_definitions(neo1 PRIVATE
    NEO1_PERSONALITY=${NEO1_PERSONALITY}
    NEO1_ENABLE_VACI=0
    NEO1_ENABLE_MSC=0
    NEO1_ENABLE_VCFFA1=0
)

target_compile_options(neo1 PRIVATE -Wall -Wextra -Werror)

target_link_libraries(neo1 PRIVATE
    neo1_soft_runner
    neo1_terminal_grid
    pico_stdlib
    pico_multicore
    pico_sync
    hardware_clocks
    hardware_dma
)

# USB-C CDC remains the console/lifecycle transport through checkpoint 3. The
# Fruit Jam USB-A host ports and UART remain untouched.
pico_enable_stdio_usb(neo1 1)
pico_enable_stdio_uart(neo1 0)
pico_add_extra_outputs(neo1)

message(STATUS
    "Neo1 Fruit Jam HSTX DVI profile: personality=${NEO1_PERSONALITY}, "
    "vaci=0, msc=0, vcffa1=0"
)
