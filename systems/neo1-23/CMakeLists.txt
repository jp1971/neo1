add_executable(neo1
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_terminal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_usb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_video.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/console_font_8x8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils.S
    ${CMAKE_SOURCE_DIR}/lib/fatfs/source/ff.c
    ${CMAKE_SOURCE_DIR}/lib/fatfs/source/ffunicode.c
    ${CMAKE_SOURCE_DIR}/lib/fatfs/source/ffsystem.c
    ${CMAKE_SOURCE_DIR}/lib/fatfs/source/diskio.c
)

target_compile_options(neo1 PRIVATE -Wall)

target_compile_definitions(neo1 PRIVATE
    DVI_DEFAULT_SERIAL_CONFIG=olimex_neo6502_cfg
    DVI_VERTICAL_REPEAT=1
    DVI_N_TMDS_BUFFERS=3
    DVI_MONOCHROME_TMDS=1
    NEO1_ENABLE_DVI=1
)

target_include_directories(neo1 PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/PicoDVI/software/include
    ${CMAKE_SOURCE_DIR}/lib/PicoDVI/software/libdvi
    ${CMAKE_SOURCE_DIR}/lib/fatfs/source
)

target_link_libraries(neo1
    pico_stdlib
    pico_multicore
    pico_util
    tinyusb_host
    tinyusb_board
    libdvi
    hardware_dma
    hardware_irq
    hardware_pio
    hardware_interp
    hardware_vreg
    hardware_clocks
)

# create map/bin/hex/uf2
pico_add_extra_outputs(neo1)

# Serial-only for now
pico_enable_stdio_usb(neo1 0)
pico_enable_stdio_uart(neo1 1)