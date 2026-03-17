from pathlib import Path

root = Path(__file__).resolve().parent
labels_path = root / "neo1_cffa1_m2_blockdrv.labels"
bin_path = root / "neo1_cffa1_m2_blockdrv.bin"
out_path = root / "neo1_cffa1_m2_blockdrv.h"

symbols = {}
for line in labels_path.read_text().splitlines():
    line = line.strip()
    if not line or line.startswith("#"):
        continue
    parts = line.split()
    if len(parts) >= 3 and parts[0] == "al":
        try:
            addr = int(parts[1], 16)
        except ValueError:
            continue
        name = parts[2].lstrip(".")
        symbols[name] = addr

cf_addr = symbols.get("CFBlockDriver", 0x1800)
tm_addr = symbols.get("TestMain", 0x1810)
data = bin_path.read_bytes()

with out_path.open("w") as f:
    f.write("// neo1_cffa1_m2_blockdrv.h\n")
    f.write("// Auto-generated from neo1_cffa1_m2_blockdrv.s\n")
    f.write(f"// CFBlockDriver entry: ${cf_addr:04X}\n")
    f.write(f"// TestMain entry:      ${tm_addr:04X}\n")
    f.write("#pragma once\n")
    f.write(f"#define NEO1_CFFA1_M2_BLOCKDRV_ADDR (0x{cf_addr:04X}u)\n")
    f.write(f"#define NEO1_CFFA1_M2_TESTMAIN_ADDR (0x{tm_addr:04X}u)\n")
    f.write(f"// {len(data)} bytes\n")
    f.write("static const unsigned char neo1_cffa1_m2_blockdrv[] = {\n")
    for i in range(0, len(data), 16):
        chunk = ", ".join(f"0x{b:02X}" for b in data[i:i + 16])
        f.write(f"    {chunk}")
        f.write(",\n" if i + 16 < len(data) else "\n")
    f.write("};\n")

print(f"Wrote {out_path} ({len(data)} bytes) CF=${cf_addr:04X} TEST=${tm_addr:04X}")
