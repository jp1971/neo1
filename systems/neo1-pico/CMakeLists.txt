# Pico runner and RP2040 service sources. RAM payloads are compiled into neo1.c
# through checked-in headers; src/hid_app.c and src/msc_app.c are not linked.
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
set(NEO1_ENABLE_MSC "1" CACHE STRING "Enable Neo1 MSC register decode (1 or 0)")
set(NEO1_ENABLE_VCFFA1 "1" CACHE STRING "Enable VCFFA1 runtime support (1 or 0)")
set(NEO1_DIAGNOSTICS "0" CACHE STRING "Enable verbose Neo1 serial diagnostics (1 or 0)")

# The Pico runner services a physical W65C02 bus and accepts only adapter 1.
if (NOT NEO1_CPU_BACKEND STREQUAL "1")
    message(FATAL_ERROR "neo1-pico requires NEO1_CPU_BACKEND=1 (wdc65C02 hardware backend)")
endif()

if (NOT NEO1_ENABLE_VACI MATCHES "^[01]$")
    message(FATAL_ERROR "NEO1_ENABLE_VACI must be 0 or 1")
endif()

if (NOT NEO1_ENABLE_MSC MATCHES "^[01]$")
    message(FATAL_ERROR "NEO1_ENABLE_MSC must be 0 or 1")
endif()

if (NEO1_ENABLE_VACI STREQUAL "1" AND NOT NEO1_ENABLE_MSC STREQUAL "1")
    message(FATAL_ERROR "NEO1_ENABLE_VACI=1 requires NEO1_ENABLE_MSC=1")
endif()

if (NOT NEO1_ENABLE_VCFFA1 MATCHES "^[01]$")
    message(FATAL_ERROR "NEO1_ENABLE_VCFFA1 must be 0 or 1")
endif()

if (NOT NEO1_DIAGNOSTICS MATCHES "^[01]$")
    message(FATAL_ERROR "NEO1_DIAGNOSTICS must be 0 or 1")
endif()

# Pico profile policy: Neo1-23 requires both RAM tools and device interfaces.
if (NEO1_PERSONALITY STREQUAL "23")
    if (NOT (NEO1_ENABLE_VACI STREQUAL "1" AND NEO1_ENABLE_MSC STREQUAL "1" AND NEO1_ENABLE_VCFFA1 STREQUAL "1"))
        message(FATAL_ERROR "NEO1_PERSONALITY=23 requires NEO1_ENABLE_VACI=1, NEO1_ENABLE_MSC=1, and NEO1_ENABLE_VCFFA1=1")
    endif()
endif()

# Normal firmware builds consume checked-in RAM image headers and never run
# assemblers or generators. Regeneration is an explicit maintenance operation,
# documented in src/ram/README.md, and requires Python plus cc65 tools.
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
    NEO1_ENABLE_MSC=${NEO1_ENABLE_MSC}
    NEO1_ENABLE_VCFFA1=${NEO1_ENABLE_VCFFA1}
    NEO1_DIAGNOSTICS=${NEO1_DIAGNOSTICS}
    NEO1_CPU_BACKEND=${NEO1_CPU_BACKEND}
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

# Emit the SDK map, binary, Intel HEX, and UF2 artifacts beside the ELF.
pico_add_extra_outputs(neo1)

# Keep SDK stdio on UART. The RP2040 USB controller is used by TinyUSB host, so
# USB device stdio is disabled.
pico_enable_stdio_usb(neo1 0)
pico_enable_stdio_uart(neo1 1)
