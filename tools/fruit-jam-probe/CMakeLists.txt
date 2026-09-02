cmake_minimum_required(VERSION 3.13)

# Use the same extension-managed SDK, toolchain, and picotool versions as the
# main Neo1 build. The helper is optional so an explicitly supplied
# PICO_SDK_PATH and toolchain remain usable outside VS Code.
if(WIN32)
    set(USERHOME "$ENV{USERPROFILE}")
else()
    set(USERHOME "$ENV{HOME}")
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.3.0)
set(NEO1_PROBE_PICO_VSCODE
    "${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake")
if(EXISTS "${NEO1_PROBE_PICO_VSCODE}")
    include("${NEO1_PROBE_PICO_VSCODE}")
endif()

set(PICO_BOARD adafruit_fruit_jam CACHE STRING "Pico SDK board")
include("${CMAKE_CURRENT_LIST_DIR}/../../pico_sdk_import.cmake")

project(neo1_fruit_jam_probe C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(neo1_fruit_jam_probe
    fruit_jam_probe.c
)

target_link_libraries(neo1_fruit_jam_probe
    pico_stdlib
    pico_unique_id
)

target_compile_definitions(neo1_fruit_jam_probe PRIVATE
    NEO1_PROBE_BOARD_NAME="${PICO_BOARD}"
)

# The USB-C device connector is the checkpoint-0 diagnostic transport. UART is
# left untouched so the probe needs no external serial adapter.
pico_enable_stdio_usb(neo1_fruit_jam_probe 1)
pico_enable_stdio_uart(neo1_fruit_jam_probe 0)
pico_add_extra_outputs(neo1_fruit_jam_probe)
