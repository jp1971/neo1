#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "runners/neo1_soft_runner.h"

enum {
    FLAG_CARRY = 0x01,
    FLAG_ZERO = 0x02,
    FLAG_INTERRUPT = 0x04,
    FLAG_DECIMAL = 0x08,
    FLAG_BREAK = 0x10,
    FLAG_OVERFLOW = 0x40,
    FLAG_NEGATIVE = 0x80,
    DECIMAL_RESULT = 0x1200,
    DECIMAL_STATUS = 0x1201,
};

static int g_failures;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", \
                    __FILE__, __LINE__, #condition); \
            g_failures++; \
        } \
    } while (0)

typedef struct {
    uint8_t rom[0x100];
    neo1_profile_t profile;
    neo1_machine_desc_t desc;
    neo1_machine_t machine;
    neo1_soft_runner_t runner;
} cpu_fixture_t;

static void fixture_prepare(cpu_fixture_t* fixture) {
    memset(fixture, 0, sizeof(*fixture));
    memset(fixture->rom, 0xEA, sizeof(fixture->rom));

    // NMI=$0300, RESET=$0200, IRQ/BRK=$0400.
    fixture->rom[0xFA] = 0x00;
    fixture->rom[0xFB] = 0x03;
    fixture->rom[0xFC] = 0x00;
    fixture->rom[0xFD] = 0x02;
    fixture->rom[0xFE] = 0x00;
    fixture->rom[0xFF] = 0x04;
    fixture->profile = (neo1_profile_t){
        .personality = NEO1_PERSONALITY_50,
        .rom = fixture->rom,
        .rom_size = sizeof(fixture->rom),
        .rom_base = 0xFF00,
        .rom_protect_base = 0xFF00,
    };
    fixture->desc = (neo1_machine_desc_t){
        .profile = &fixture->profile,
    };
    CHECK(neo1_machine_init(&fixture->machine, &fixture->desc));
}

static void fixture_start(cpu_fixture_t* fixture) {
    CHECK(neo1_soft_runner_init(&fixture->runner, &fixture->machine));
}

static void fixture_finish(cpu_fixture_t* fixture) {
    neo1_soft_runner_discard(&fixture->runner);
}

static void load_bytes(cpu_fixture_t* fixture, uint16_t address,
                       const uint8_t* bytes, size_t count) {
    memcpy(&fixture->machine.ram[address], bytes, count);
}

static void test_reset_vector_and_memory_ownership(void) {
    cpu_fixture_t fixture;
    fixture_prepare(&fixture);
    const uint8_t program[] = {
        0xA9, 0x5A,       // LDA #$5A
        0x8D, 0x00, 0x10 // STA $1000
    };
    load_bytes(&fixture, 0x0200, program, sizeof(program));
    fixture.machine.ram[0x0000] = 0x12;
    fixture.machine.ram[0x0001] = 0x34;
    fixture.machine.ram[0x0002] = 0x56;

    fixture_start(&fixture);
    CHECK(fixture.machine.ram[0x0000] == 0x12);
    CHECK(fixture.machine.ram[0x0001] == 0x34);
    CHECK(fixture.machine.ram[0x0002] == 0x56);
    CHECK(neo1_soft_runner_step(&fixture.runner) == 2);
    CHECK(neo1_soft_runner_step(&fixture.runner) == 4);
    CHECK(fixture.machine.ram[0x1000] == 0x5A);
    fixture_finish(&fixture);
}

static void test_irq_masking_and_rti(void) {
    cpu_fixture_t fixture;
    fixture_prepare(&fixture);
    const uint8_t program[] = {
        0x78,             // SEI
        0xA9, 0x11,       // LDA #$11
        0x8D, 0x00, 0x10, // STA $1000
        0x58,             // CLI
        0xEA,             // NOP (IRQ return address)
        0xA9, 0x22,       // LDA #$22
        0x8D, 0x02, 0x10, // STA $1002
    };
    const uint8_t handler[] = {
        0xEE, 0x01, 0x10, // INC $1001
        0x40,             // RTI
    };
    load_bytes(&fixture, 0x0200, program, sizeof(program));
    load_bytes(&fixture, 0x0400, handler, sizeof(handler));
    fixture.machine.ram[0x1001] = 0;

    fixture_start(&fixture);
    neo1_soft_runner_set_irq(&fixture.runner, true);
    CHECK(neo1_soft_runner_step(&fixture.runner) == 2); // SEI
    CHECK(neo1_soft_runner_step(&fixture.runner) == 2); // LDA
    CHECK(neo1_soft_runner_step(&fixture.runner) == 4); // STA
    CHECK(neo1_soft_runner_step(&fixture.runner) == 2); // CLI
    CHECK(fixture.machine.ram[0x1000] == 0x11);

    // W65C02 IRQ recognition after CLI is delayed through the following NOP.
    // The step then includes seven-cycle entry and the six-cycle handler INC.
    CHECK(neo1_soft_runner_step(&fixture.runner) == 15);
    CHECK(fixture.machine.ram[0x1001] == 1);
    CHECK(fixture.machine.ram[0x01FD] == 0x02);
    CHECK(fixture.machine.ram[0x01FC] == 0x08);
    CHECK((fixture.machine.ram[0x01FB] & FLAG_BREAK) == 0);
    neo1_soft_runner_set_irq(&fixture.runner, false);

    CHECK(neo1_soft_runner_step(&fixture.runner) == 6); // RTI
    CHECK(neo1_soft_runner_step(&fixture.runner) == 2); // LDA
    CHECK(neo1_soft_runner_step(&fixture.runner) == 4); // STA
    CHECK(fixture.machine.ram[0x1002] == 0x22);
    fixture_finish(&fixture);
}

static void test_nmi_and_rti(void) {
    cpu_fixture_t fixture;
    fixture_prepare(&fixture);
    const uint8_t program[] = {
        0xEA,             // NOP (NMI return address)
        0xA9, 0x33,       // LDA #$33
        0x8D, 0x03, 0x10, // STA $1003
    };
    const uint8_t handler[] = {
        0xEE, 0x04, 0x10, // INC $1004
        0x40,             // RTI
    };
    load_bytes(&fixture, 0x0200, program, sizeof(program));
    load_bytes(&fixture, 0x0300, handler, sizeof(handler));
    fixture.machine.ram[0x1004] = 0;

    fixture_start(&fixture);
    neo1_soft_runner_nmi(&fixture.runner);
    // The NMI edge is presented with the NOP already on the bus, so the W65C02
    // completes that instruction before seven-cycle entry and the handler INC.
    CHECK(neo1_soft_runner_step(&fixture.runner) == 15);
    CHECK(fixture.machine.ram[0x1004] == 1);
    CHECK(fixture.machine.ram[0x01FD] == 0x02);
    CHECK(fixture.machine.ram[0x01FC] == 0x01);
    CHECK((fixture.machine.ram[0x01FB] & FLAG_BREAK) == 0);
    CHECK(neo1_soft_runner_step(&fixture.runner) == 6); // RTI
    CHECK(neo1_soft_runner_step(&fixture.runner) == 2); // LDA
    CHECK(neo1_soft_runner_step(&fixture.runner) == 4); // STA
    CHECK(fixture.machine.ram[0x1003] == 0x33);
    fixture_finish(&fixture);
}

static void test_brk_stack_and_rti(void) {
    cpu_fixture_t fixture;
    fixture_prepare(&fixture);
    const uint8_t program[] = {
        0x00, 0xEA,       // BRK plus signature byte
        0xA9, 0x44,       // LDA #$44
        0x8D, 0x05, 0x10, // STA $1005
    };
    const uint8_t handler[] = {
        0xEE, 0x06, 0x10, // INC $1006
        0x40,             // RTI
    };
    load_bytes(&fixture, 0x0200, program, sizeof(program));
    load_bytes(&fixture, 0x0400, handler, sizeof(handler));
    fixture.machine.ram[0x1006] = 0;

    fixture_start(&fixture);
    CHECK(neo1_soft_runner_step(&fixture.runner) == 7);
    CHECK(fixture.machine.ram[0x01FD] == 0x02);
    CHECK(fixture.machine.ram[0x01FC] == 0x02);
    CHECK((fixture.machine.ram[0x01FB] & FLAG_BREAK) != 0);
    CHECK(neo1_soft_runner_step(&fixture.runner) == 6); // INC
    CHECK(neo1_soft_runner_step(&fixture.runner) == 6); // RTI
    CHECK(neo1_soft_runner_step(&fixture.runner) == 2); // LDA
    CHECK(neo1_soft_runner_step(&fixture.runner) == 4); // STA
    CHECK(fixture.machine.ram[0x1005] == 0x44);
    CHECK(fixture.machine.ram[0x1006] == 1);
    fixture_finish(&fixture);
}

static void run_decimal_case(uint8_t opcode, uint8_t carry_opcode,
                             uint8_t accumulator, uint8_t operand,
                             uint8_t expected_result, uint8_t expected_flags) {
    cpu_fixture_t fixture;
    fixture_prepare(&fixture);
    const uint8_t program[] = {
        0xF8,                   // SED
        carry_opcode,           // CLC or SEC
        0xA9, accumulator,      // LDA #accumulator
        opcode, operand,        // ADC/SBC #operand
        0x8D, 0x00, 0x12,      // STA DECIMAL_RESULT
        0x08,                   // PHP
        0x68,                   // PLA
        0x8D, 0x01, 0x12,      // STA DECIMAL_STATUS
    };
    load_bytes(&fixture, 0x0200, program, sizeof(program));

    fixture_start(&fixture);
    for (size_t i = 0; i < 8; i++) {
        (void)neo1_soft_runner_step(&fixture.runner);
    }
    const uint8_t arithmetic_flags = FLAG_CARRY | FLAG_ZERO | FLAG_DECIMAL |
                                     FLAG_OVERFLOW | FLAG_NEGATIVE;
    CHECK(fixture.machine.ram[DECIMAL_RESULT] == expected_result);
    CHECK((fixture.machine.ram[DECIMAL_STATUS] & arithmetic_flags) ==
          expected_flags);
    fixture_finish(&fixture);
}

static void test_decimal_adc_sbc_flags(void) {
    run_decimal_case(0x69, 0x18, 0x15, 0x27, 0x42, FLAG_DECIMAL);
    run_decimal_case(0x69, 0x18, 0x99, 0x01, 0x00,
                     FLAG_CARRY | FLAG_ZERO | FLAG_DECIMAL);
    run_decimal_case(0x69, 0x18, 0x50, 0x50, 0x00,
                     FLAG_CARRY | FLAG_ZERO | FLAG_DECIMAL | FLAG_OVERFLOW);
    run_decimal_case(0xE9, 0x38, 0x50, 0x01, 0x49,
                     FLAG_CARRY | FLAG_DECIMAL);
    run_decimal_case(0xE9, 0x38, 0x00, 0x01, 0x99,
                     FLAG_DECIMAL | FLAG_NEGATIVE);
}

static void test_selected_w65c02_instructions_and_cycles(void) {
    cpu_fixture_t fixture;
    fixture_prepare(&fixture);
    const uint8_t program[] = {
        0xA2, 0x5A,       // LDX #$5A
        0xA0, 0xA5,       // LDY #$A5
        0xDA,             // PHX
        0x5A,             // PHY
        0xA2, 0x00,       // LDX #$00
        0xA0, 0x00,       // LDY #$00
        0x7A,             // PLY
        0xFA,             // PLX
        0x8E, 0x10, 0x10, // STX $1010
        0x8C, 0x11, 0x10, // STY $1011
        0xA9, 0xFF,       // LDA #$FF
        0x85, 0x20,       // STA $20
        0x64, 0x20,       // STZ $20
        0xA2, 0x02,       // LDX #$02
        0x9D, 0x00, 0x11, // STA $1100,X
        0x9E, 0x00, 0x11, // STZ $1100,X
    };
    const uint32_t expected_cycles[] = {
        2, 2, 3, 3, 2, 2, 4, 4, 4, 4, 2, 3, 3, 2, 5, 5,
    };
    load_bytes(&fixture, 0x0200, program, sizeof(program));

    fixture_start(&fixture);
    for (size_t i = 0; i < sizeof(expected_cycles) / sizeof(expected_cycles[0]);
         i++) {
        CHECK(neo1_soft_runner_step(&fixture.runner) == expected_cycles[i]);
    }
    CHECK(fixture.machine.ram[0x1010] == 0x5A);
    CHECK(fixture.machine.ram[0x1011] == 0xA5);
    CHECK(fixture.machine.ram[0x0020] == 0x00);
    CHECK(fixture.machine.ram[0x1102] == 0x00);
    fixture_finish(&fixture);
}

int main(void) {
    test_reset_vector_and_memory_ownership();
    test_irq_masking_and_rti();
    test_nmi_and_rti();
    test_brk_stack_and_rti();
    test_decimal_adc_sbc_flags();
    test_selected_w65c02_instructions_and_cycles();

    if (g_failures != 0) {
        fprintf(stderr, "neo1_soft_cpu_tests: %d failure(s)\n", g_failures);
        return 1;
    }

    puts("neo1_soft_cpu_tests: reset, interrupts, decimal, and selected "
         "W65C02 operations pass");
    return 0;
}
