add_executable(apple1
    ${CMAKE_CURRENT_SOURCE_DIR}/src/apple1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_terminal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/apple1_usb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_video.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/console_font_8x8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils.S
)

target_compile_options(apple1 PRIVATE -Wall)

target_compile_definitions(apple1 PRIVATE
    OLIMEX_NEO6502=1
    DVI_DEFAULT_SERIAL_CONFIG=olimex_neo6502_cfg
    DVI_VERTICAL_REPEAT=1
    DVI_N_TMDS_BUFFERS=3
    DVI_MONOCHROME_TMDS=1
)

if (ENABLE_DVI)
    target_compile_definitions(apple1 PRIVATE
        APPLE1_ENABLE_DVI=1
    )
endif()

target_link_libraries(apple1
    pico_stdlib
    pico_multicore
    pico_util
    tinyusb_host
    tinyusb_board
)

if (ENABLE_DVI)
    target_link_libraries(apple1
        libdvi
        hardware_dma
        hardware_irq
        hardware_pio
        hardware_interp
        hardware_vreg
        hardware_clocks
    )
endif()

# create map/bin/hex/uf2
pico_add_extra_outputs(apple1)

# Serial-only for now
pico_enable_stdio_usb(apple1 0)
pico_enable_stdio_uart(apple1 1)