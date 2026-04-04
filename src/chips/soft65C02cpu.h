#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
	uint16_t addr;
	bool rw;
	uint8_t data;
	bool irq;
} soft65c02cpu_t;

#define MOS6502CPU_T                    soft65c02cpu_t
#define MOS6502CPU_INIT(c, desc)        soft65C02cpu_init((c), (desc))
#define MOS6502CPU_RESET(c)             soft65C02cpu_reset(c)
#define MOS6502CPU_NMI(c)               soft65C02cpu_nmi(c)
#define MOS6502CPU_TICK(c)              soft65C02cpu_tick(c)
#define MOS6502CPU_GET_ADDR(c)          ((c)->addr)
#define MOS6502CPU_GET_DATA(c)          ((c)->data)
#define MOS6502CPU_SET_DATA(c, d)       soft65C02cpu_set_data((c), (d))
#define MOS6502CPU_SET_IRQ(c, state)    soft65C02cpu_set_irq((c), (state))
#define MOS6502CPU_SET_RESET(c, state)  soft65C02cpu_set_reset((c), (state))
#define MOS6502CPU_NEEDS_EXTERNAL_BUS   (0)

#ifdef __cplusplus
extern "C" {
#endif

void soft65C02cpu_init(soft65c02cpu_t* c, void* user);
void soft65C02cpu_reset(soft65c02cpu_t* c);
void soft65C02cpu_nmi(soft65c02cpu_t* c);
void soft65C02cpu_tick(soft65c02cpu_t* c);
void soft65C02cpu_set_data(soft65c02cpu_t* c, uint8_t data);
void soft65C02cpu_set_irq(soft65c02cpu_t* c, bool state);
void soft65C02cpu_set_reset(soft65c02cpu_t* c, bool state);

// Implemented by neo1 runtime to bridge fake65c02 core memory accesses.
uint8_t neo1_soft65c02_mem_read(void* user, uint16_t addr);
void neo1_soft65c02_mem_write(void* user, uint16_t addr, uint8_t data);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef CHIPS_IMPL

static void* _soft65c02_user = 0;
static soft65c02cpu_t* _soft65c02_active = 0;

// fake65c02 core interface
unsigned char read6502(unsigned short address) {
	uint8_t data = neo1_soft65c02_mem_read(_soft65c02_user, (uint16_t)address);
	if (_soft65c02_active) {
		_soft65c02_active->addr = (uint16_t)address;
		_soft65c02_active->rw = true;
		_soft65c02_active->data = data;
	}
	return data;
}

void write6502(unsigned short address, unsigned char value) {
	neo1_soft65c02_mem_write(_soft65c02_user, (uint16_t)address, (uint8_t)value);
	if (_soft65c02_active) {
		_soft65c02_active->addr = (uint16_t)address;
		_soft65c02_active->rw = false;
		_soft65c02_active->data = (uint8_t)value;
	}
}

#include "fake65c02.h"

void soft65C02cpu_init(soft65c02cpu_t* c, void* user) {
	_soft65c02_user = user;
	c->addr = 0;
	c->rw = true;
	c->data = 0;
	c->irq = false;
	reset6502();
}

void soft65C02cpu_reset(soft65c02cpu_t* c) {
	(void)c;
	reset6502();
}

void soft65C02cpu_nmi(soft65c02cpu_t* c) {
	(void)c;
	nmi6502();
}

void soft65C02cpu_tick(soft65c02cpu_t* c) {
	_soft65c02_active = c;
	if (c->irq) {
		irq6502();
	}
	step6502();
	_soft65c02_active = 0;
}

void soft65C02cpu_set_data(soft65c02cpu_t* c, uint8_t data) {
	c->data = data;
}

void soft65C02cpu_set_irq(soft65c02cpu_t* c, bool state) {
	c->irq = state;
}

void soft65C02cpu_set_reset(soft65c02cpu_t* c, bool state) {
	if (state) {
		soft65C02cpu_reset(c);
	}
}

#endif
