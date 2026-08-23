# RAM Artifacts (`src/ram`)

This directory contains 6502 RAM-resident payload sources and the checked-in C
headers consumed by firmware builds. Normal CMake builds do not assemble or
regenerate these payloads.

## Source of Truth

- `neo1_vaci_v1.s`: VACI utility code image source.
- `neo1_cffa1_m2_blockdrv.s`: VCFFA1 M2 block driver source.
- `*.cfg`: linker/config files for corresponding assembly payloads.
- `build_vaci.py`: supported VACI check/update entry point; assembles into a
  temporary directory and does not consume adjacent `.bin` files.
- `gen_vaci_header.py`: low-level VACI binary-to-header formatter used by the
  supported build script.
- `gen_cffa1_m2_header.py`: canonical VCFFA1 labels/binary-to-header formatter.
- `gen_m2_header.py`: older fixed-path duplicate with no current consumer;
  retained as a legacy/reference candidate.

## Checked-in Build Inputs

Firmware directly includes these generated headers:

- `neo1_vaci_v1.h`
- `neo1_cffa1_m2_blockdrv.h`

Edit the `.s`, `.cfg`, or generator scripts and explicitly regenerate the
corresponding header. Do not edit byte arrays by hand.

## Ignored Intermediates

Local assembly products such as the following are not firmware build inputs and
may be absent or stale:

- `*.bin`, `*.o`, `*.map`, `*.lst`, `*.labels`

## Regeneration Examples

Assemble VACI and update its checked-in image header (requires cc65):

```sh
python3 src/ram/build_vaci.py --update
```

Verify that the source and checked-in header agree:

```sh
python3 src/ram/build_vaci.py --check
```

For low-level use with a freshly assembled, explicitly selected VACI binary:

```sh
python3 src/ram/gen_vaci_header.py \
	--bin src/ram/neo1_vaci_v1.bin \
	--out src/ram/neo1_vaci_v1.h
```

Assemble and link VCFFA1, then regenerate its header (requires cc65):

```sh
ca65 src/ram/neo1_cffa1_m2_blockdrv.s \
	-o src/ram/neo1_cffa1_m2_blockdrv.o
ld65 -C src/ram/neo1_cffa1_m2.cfg \
	-o src/ram/neo1_cffa1_m2_blockdrv.bin \
	src/ram/neo1_cffa1_m2_blockdrv.o \
	-Ln src/ram/neo1_cffa1_m2_blockdrv.labels \
	-m src/ram/neo1_cffa1_m2_blockdrv.map
python3 src/ram/gen_cffa1_m2_header.py \
	--labels src/ram/neo1_cffa1_m2_blockdrv.labels \
	--bin src/ram/neo1_cffa1_m2_blockdrv.bin \
	--out src/ram/neo1_cffa1_m2_blockdrv.h
```

The repository has no combined VCFFA1 check/update wrapper; compare the
regenerated header before committing it.
