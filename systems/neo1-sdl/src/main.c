#include "neo1_platform.h"

#include <stddef.h>
#include <stdint.h>

int main(void) {
    const int window_width = 560;
    const int window_height = 384;

    neo1_platform_init(window_width, window_height, "Neo1 Host (M0)");

    while (!neo1_platform_should_quit()) {
        uint8_t keycode = 0;
        bool pressed = false;

        neo1_platform_poll_key(&keycode, &pressed);
        neo1_platform_update_display(NULL, 0, 0);
    }

    neo1_platform_shutdown();
    return 0;
}