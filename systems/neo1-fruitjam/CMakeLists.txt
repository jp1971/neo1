# RP2350 software-CPU runner with Fruit Jam-owned HSTX DVI and PIO USB-host
# keyboard. USB-C CDC remains available for diagnostics and fallback input;
# audio, storage, VACI, VCFFA1, PicoDVI, and physical-W65C02 services stay out.
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
    src/neo1_usb_host.c
)

target_include_directories(neo1 BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(neo1 PRIVATE
    NEO1_PERSONALITY=${NEO1_PERSONALITY}
    NEO1_ENABLE_VACI=0
    NEO1_ENABLE_MSC=0
    NEO1_ENABLE_VCFFA1=0
    # Linking TinyUSB host makes pico_stdio_usb assume the application owns
    # all TinyUSB initialization. Neo1 still uses the SDK's standard CDC
    # descriptors. Fruit Jam initializes native device root port 0 and PIO host
    # root port 1 explicitly; a no-argument SDK TinyUSB init would initialize
    # both roots before the host's pins and DMA channel are configured.
    PICO_STDIO_USB_USE_DEFAULT_DESCRIPTORS=1
    PICO_STDIO_USB_ENABLE_TINYUSB_INIT=0
    PICO_STDIO_USB_ENABLE_IRQ_BACKGROUND_TASK=0
)

target_compile_options(neo1 PRIVATE
    -Wall
    -Wextra
    -Werror
    # Pico-PIO-USB 0.7.2 combines `inline` with the SDK 2.3 RP2350
    # time-critical macro, which GCC reports as a conflicting noinline
    # attribute. Keep the pinned dependency unmodified and contain the
    # third-party compatibility exception here.
    -Wno-error=attributes
)

target_link_libraries(neo1 PRIVATE
    neo1_soft_runner
    neo1_hid_keyboard
    neo1_terminal_grid
    pico_stdlib
    pico_multicore
    pico_sync
    hardware_clocks
    hardware_dma
    hardware_gpio
    hardware_pio
    tinyusb_host
    tinyusb_pico_pio_usb
)

# Native USB-C stays in device mode while Pico-PIO-USB supplies host root port
# 1 on the onboard USB-A hub.
pico_enable_stdio_usb(neo1 1)
pico_enable_stdio_uart(neo1 0)
pico_add_extra_outputs(neo1)

message(STATUS
    "Neo1 Fruit Jam HSTX DVI + USB keyboard profile: personality=${NEO1_PERSONALITY}, "
    "vaci=0, msc=0, vcffa1=0"
)
