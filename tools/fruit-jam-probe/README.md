# Fruit Jam checkpoint-0 probe

This standalone program proves that Neo1's supported Pico SDK and toolchain can
select, compile for, flash, and identify the official
`adafruit_fruit_jam` RP2350B board. It deliberately contains no Neo1 machine,
software-CPU, display, input, or storage code.

Configure and build from the repository root:

```sh
cmake -S tools/fruit-jam-probe -B build-fruit-jam-probe
cmake --build build-fruit-jam-probe
```

The build uses the Raspberry Pi Pico VS Code extension's SDK 2.3.0, Arm GNU
Toolchain 13.3.Rel1, and picotool 2.3.0 when they are installed. An explicitly
supplied `PICO_SDK_PATH` and compatible Arm toolchain may be used instead.

To flash, hold **BOOT/BOOTSEL** while connecting or resetting the Fruit Jam,
then copy `build-fruit-jam-probe/neo1_fruit_jam_probe.uf2` to the mounted
`RP2350` volume, or use picotool:

```sh
picotool load -f -x build-fruit-jam-probe/neo1_fruit_jam_probe.uf2
```

Flashing replaces the board's current firmware; it can be restored later by
flashing the corresponding UF2. Open the USB CDC serial port after the board
reboots. The probe reports its SDK, unique board ID, memory sizes, and the
official SDK pin definitions used by later checkpoints, then emits a heartbeat
once per second.
