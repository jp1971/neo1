# USB HID boot-keyboard report decoding shared by hardware targets. USB stack
# initialization, callbacks, and machine injection remain target-owned.
add_library(neo1_hid_keyboard STATIC
    neo1_hid_keyboard.c
)

target_include_directories(neo1_hid_keyboard PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_options(neo1_hid_keyboard PRIVATE -Wall -Wextra -Werror)
