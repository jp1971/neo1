#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "input/neo1_hid_keyboard.h"

static int failures;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", \
                    __FILE__, __LINE__, #condition); \
            failures++; \
        } \
    } while (0)

typedef struct event_log {
    neo1_hid_event_t events[16];
    size_t count;
} event_log_t;

static void record_event(const neo1_hid_event_t* event, void* user_data) {
    event_log_t* log = user_data;
    if (log->count < sizeof(log->events) / sizeof(log->events[0])) {
        log->events[log->count++] = *event;
    }
}

static void report(
    neo1_hid_keyboard_t* keyboard,
    event_log_t* log,
    uint8_t modifiers,
    uint8_t a,
    uint8_t b)
{
    const uint8_t keys[NEO1_HID_KEY_COUNT] = {a, b, 0, 0, 0, 0};
    neo1_hid_keyboard_process(
        keyboard, modifiers, keys, record_event, log);
}

static void test_ascii_and_edges(void) {
    neo1_hid_keyboard_t keyboard;
    event_log_t log = {0};
    neo1_hid_keyboard_reset(&keyboard);

    report(&keyboard, &log, 0, 0x04, 0);
    report(&keyboard, &log, 0, 0x04, 0);
    report(&keyboard, &log, 0, 0, 0);
    report(&keyboard, &log, 1u << 1, 0x04, 0x1E);

    CHECK(log.count == 3);
    CHECK(log.events[0].kind == NEO1_HID_EVENT_CHARACTER);
    CHECK(log.events[0].character == 'a');
    CHECK(log.events[1].character == 'A');
    CHECK(log.events[2].character == '!');
}

static void test_controls_and_special_keys(void) {
    neo1_hid_keyboard_t keyboard;
    event_log_t log = {0};
    neo1_hid_keyboard_reset(&keyboard);

    report(&keyboard, &log, 1u << 0, 0x15, 0); // Ctrl-R
    report(&keyboard, &log, 0, 0, 0);
    report(&keyboard, &log, 0, 0x28, 0x2A); // Return + Backspace
    report(&keyboard, &log, 0, 0, 0);
    report(&keyboard, &log, 0, 0x2B, 0x2C); // Tab + Space
    report(&keyboard, &log, 0, 0, 0);
    report(&keyboard, &log, 0, 0x45, 0); // F12

    CHECK(log.count == 6);
    CHECK(log.events[0].character == 0x12);
    CHECK(log.events[1].character == '\r');
    CHECK(log.events[2].character == '\b');
    CHECK(log.events[3].character == '\t');
    CHECK(log.events[4].character == ' ');
    CHECK(log.events[5].kind == NEO1_HID_EVENT_F12);
}

static void test_reset_for_reconnect(void) {
    neo1_hid_keyboard_t keyboard;
    event_log_t log = {0};
    neo1_hid_keyboard_reset(&keyboard);

    report(&keyboard, &log, 0, 0x05, 0);
    neo1_hid_keyboard_reset(&keyboard);
    report(&keyboard, &log, 0, 0x05, 0);

    CHECK(log.count == 2);
    CHECK(log.events[0].character == 'b');
    CHECK(log.events[1].character == 'b');
}

int main(void) {
    test_ascii_and_edges();
    test_controls_and_special_keys();
    test_reset_for_reconnect();

    if (failures != 0) {
        fprintf(stderr, "neo1_hid_keyboard_tests: %d failure(s)\n", failures);
        return 1;
    }
    puts("neo1_hid_keyboard_tests: shared HID translation preserved");
    return 0;
}
