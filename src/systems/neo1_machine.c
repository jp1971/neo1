#include "systems/neo1_machine.h"

#include <string.h>

static bool neo1_machine_port_attached(const neo1_device_port_t* port) {
    return port->read && port->write;
}

static bool neo1_machine_cffa1_handles_addr(uint16_t addr) {
    return (addr == NEO1_CFFA1_ID1_ADDR) ||
           (addr == NEO1_CFFA1_ID2_ADDR) ||
           ((addr >= NEO1_CFFA1_IO_BASE) && (addr <= NEO1_CFFA1_IO_END));
}

bool neo1_machine_init(neo1_machine_t* machine, const neo1_machine_desc_t* desc) {
    if (!machine || !desc || !desc->profile || !desc->profile->rom ||
        (desc->profile->rom_size == 0) ||
        (desc->profile->rom_protect_base > desc->profile->rom_base) ||
        (((uint32_t)desc->profile->rom_base +
          (uint32_t)desc->profile->rom_size) > NEO1_MACHINE_MEM_SIZE))
    {
        return false;
    }

    memset(machine, 0, sizeof(*machine));
    machine->profile = desc->profile;
    machine->rom_base = desc->profile->rom_base;
    machine->rom_protect_base = desc->profile->rom_protect_base;
    machine->char_out = desc->char_out;
    machine->char_out_user_data = desc->char_out_user_data;
    machine->msc = desc->msc;
    machine->vcffa1 = desc->vcffa1;

    for (uint32_t addr = 0; addr < NEO1_MACHINE_MEM_SIZE; addr += 2) {
        machine->ram[addr] = 0x00;
        machine->ram[addr + 1] = 0xFF;
    }
    memcpy(&machine->ram[desc->profile->rom_base],
           desc->profile->rom,
           desc->profile->rom_size);
    neo1_machine_reset(machine);
    return true;
}

void neo1_machine_reset(neo1_machine_t* machine) {
    neo1_apple1_pia_reset(&machine->pia);
}

uint8_t neo1_machine_read(neo1_machine_t* machine, uint16_t addr) {
    if (neo1_machine_port_attached(&machine->vcffa1) &&
        neo1_machine_cffa1_handles_addr(addr))
    {
        return machine->vcffa1.read(machine->vcffa1.user_data, addr);
    }

    if (neo1_apple1_pia_handles_addr(addr)) {
        return neo1_apple1_pia_read(&machine->pia, addr);
    }

    if (neo1_machine_port_attached(&machine->msc)) {
        switch (addr) {
            case NEO1_IO_MSC_STATUS:
            case NEO1_IO_MSC_DATA:
            case NEO1_IO_MSC_INDEX:
            case NEO1_IO_MSC_INFO:
            case NEO1_IO_MSC_SIZE_LO:
            case NEO1_IO_MSC_SIZE_HI:
                return machine->msc.read(machine->msc.user_data, addr);
            default:
                break;
        }
    }

    return machine->ram[addr];
}

void neo1_machine_write(neo1_machine_t* machine, uint16_t addr, uint8_t data) {
    if (neo1_machine_port_attached(&machine->vcffa1) &&
        neo1_machine_cffa1_handles_addr(addr))
    {
        machine->vcffa1.write(machine->vcffa1.user_data, addr, data);
        return;
    }

    if (neo1_apple1_pia_handles_addr(addr)) {
        uint8_t display_byte = 0;
        if (neo1_apple1_pia_write(&machine->pia, addr, data, &display_byte) &&
            machine->char_out)
        {
            machine->char_out(display_byte, machine->char_out_user_data);
        }
        return;
    }

    if (neo1_machine_port_attached(&machine->msc)) {
        switch (addr) {
            case NEO1_IO_MSC_CMD:
            case NEO1_IO_MSC_SECTOR_LO:
            case NEO1_IO_MSC_SECTOR_HI:
            case NEO1_IO_MSC_DATA:
            case NEO1_IO_MSC_INDEX:
            case NEO1_IO_MSC_SIZE_LO:
            case NEO1_IO_MSC_SIZE_HI:
                machine->msc.write(machine->msc.user_data, addr, data);
                return;
            default:
                break;
        }
    }

    if (addr < machine->rom_protect_base) {
        machine->ram[addr] = data;
    }
}

void neo1_machine_key_down(neo1_machine_t* machine, uint8_t ascii) {
    neo1_apple1_pia_key_down(&machine->pia, ascii);
}
