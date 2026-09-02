#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/unique_id.h"

#ifndef ADAFRUIT_FRUIT_JAM
#error "The Neo1 Fruit Jam probe must be built for PICO_BOARD=adafruit_fruit_jam"
#endif

#ifndef NEO1_PROBE_BOARD_NAME
#define NEO1_PROBE_BOARD_NAME "unknown"
#endif

static void print_probe_identity(void) {
    char board_id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    pico_get_unique_board_id_string(board_id, sizeof(board_id));

    printf("\n[neo1-fruitjam-probe] ready\n");
    printf("[neo1-fruitjam-probe] board=%s platform=rp2350 variant=RP2350B\n",
           NEO1_PROBE_BOARD_NAME);
    printf("[neo1-fruitjam-probe] sdk=%s board_id=%s\n",
           PICO_SDK_VERSION_STRING, board_id);
    printf("[neo1-fruitjam-probe] flash=%u psram=%u led=%u\n",
           (unsigned)PICO_FLASH_SIZE_BYTES,
           (unsigned)PICO_PSRAM_SIZE_BYTES,
           (unsigned)PICO_DEFAULT_LED_PIN);
    printf("[neo1-fruitjam-probe] dvi=%u-%u usb_host_dp=%u power=%u\n",
           (unsigned)ADAFRUIT_FRUIT_JAM_DVI_CKN_PIN,
           (unsigned)ADAFRUIT_FRUIT_JAM_DVI_D2P_PIN,
           (unsigned)ADAFRUIT_FRUIT_JAM_USB_HOST_DATA_PLUS_PIN,
           (unsigned)ADAFRUIT_FRUIT_JAM_USB_HOST_5V_POWER_PIN);
    printf("[neo1-fruitjam-probe] sd_clk=%u sd_cmd=%u sd_dat0=%u sd_detect=%u\n",
           (unsigned)ADAFRUIT_FRUIT_JAM_SDIO_CLOCK_PIN,
           (unsigned)ADAFRUIT_FRUIT_JAM_SDIO_COMMAND_PIN,
           (unsigned)ADAFRUIT_FRUIT_JAM_SDIO_DATA0_PIN,
           (unsigned)ADAFRUIT_FRUIT_JAM_SD_CARD_DETECT_PIN);
}

int main(void) {
    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // Give the USB CDC device time to enumerate before the one-shot identity
    // block. A heartbeat follows so a late-opened monitor can confirm liveness.
    sleep_ms(1500);
    print_probe_identity();

    bool led_on = false;
    unsigned heartbeat = 0;
    while (true) {
        led_on = !led_on;
        gpio_put(PICO_DEFAULT_LED_PIN, led_on);
        printf("[neo1-fruitjam-probe] heartbeat=%u\n", heartbeat++);
        sleep_ms(1000);
    }
}
