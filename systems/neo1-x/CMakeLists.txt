add_executable(neo1
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_terminal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_usb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_msc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_cffa1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_video.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils.S
    ${CMAKE_SOURCE_DIR}/lib/fatfs/source/ff.c
    ${CMAKE_SOURCE_DIR}/lib/fatfs/source/ffunicode.c
    ${CMAKE_SOURCE_DIR}/lib/fatfs/source/ffsystem.c
    ${CMAKE_SOURCE_DIR}/lib/fatfs/source/diskio.c
)

# Personality and feature toggles are exposed as cache variables so they can be
# switched via CMake configure presets or one-off -D flags.
set(NEO1_PERSONALITY "50" CACHE STRING "Neo1 personality: 23 or 50")
set_property(CACHE NEO1_PERSONALITY PROPERTY STRINGS 23 50)
set(NEO1_ENABLE_VACI "1" CACHE STRING "Enable VACI runtime support (1 or 0)")
set(NEO1_ENABLE_VCFFA1 "1" CACHE STRING "Enable VCFFA1 runtime support (1 or 0)")

if (NOT NEO1_ENABLE_VACI MATCHES "^[01]$")
    message(FATAL_ERROR "NEO1_ENABLE_VACI must be 0 or 1")
endif()

if (NOT NEO1_ENABLE_VCFFA1 MATCHES "^[01]$")
    message(FATAL_ERROR "NEO1_ENABLE_VCFFA1 must be 0 or 1")
endif()

# Policy: Neo1-23 always ships with both VACI and VCFFA1 enabled.
if (NEO1_PERSONALITY STREQUAL "23")
    if (NOT (NEO1_ENABLE_VACI STREQUAL "1" AND NEO1_ENABLE_VCFFA1 STREQUAL "1"))
        message(FATAL_ERROR "NEO1_PERSONALITY=23 requires NEO1_ENABLE_VACI=1 and NEO1_ENABLE_VCFFA1=1")
    endif()
endif()

# Use checked-in RAM image headers instead of auto-building from assembly.
# This simplifies the build and avoids the need for ca65/ld65/python3 toolchains,
# and eliminates the power-cycle issue that occurred after rebuilding the headers.
target_include_directories(neo1 BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/src/ram)

target_compile_options(neo1 PRIVATE -Wall)

target_compile_definitions(neo1 PRIVATE
    # PicoDVI/serial timing config.
    DVI_DEFAULT_SERIAL_CONFIG=olimex_neo6502_cfg
    DVI_VERTICAL_REPEAT=1
    DVI_N_TMDS_BUFFERS=3
    DVI_MONOCHROME_TMDS=1
    NEO1_ENABLE_DVI=1

    # Runtime personality/features passed through to firmware C sources.
    NEO1_PERSONALITY=${NEO1_PERSONALITY}
    NEO1_ENABLE_VACI=${NEO1_ENABLE_VACI}
    NEO1_ENABLE_VCFFA1=${NEO1_ENABLE_VCFFA1}
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