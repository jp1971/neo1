from pathlib import Path
import argparse


def read_symbol_address(labels_path: Path, symbol_name: str, default_addr: int) -> int:
    for line in labels_path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) < 3 or parts[0] != "al":
            continue
        name = parts[2].lstrip(".")
        if name != symbol_name:
            continue
        try:
            return int(parts[1], 16)
        except ValueError:
            continue
    return default_addr


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate neo1_cffa1_m2_blockdrv.h from labels+binary")
    parser.add_argument("--labels", required=True, dest="labels_path")
    parser.add_argument("--bin", required=True, dest="bin_path")
    parser.add_argument("--out", required=True, dest="out_path")
    args = parser.parse_args()

    labels_path = Path(args.labels_path)
    bin_path = Path(args.bin_path)
    out_path = Path(args.out_path)

    cf_addr = read_symbol_address(labels_path, "CFBlockDriver", 0x1800)
    testmain_addr = read_symbol_address(labels_path, "TestMain", 0x1810)
    data = bin_path.read_bytes()

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        f.write("// neo1_cffa1_m2_blockdrv.h\n")
        f.write("// Auto-generated from neo1_cffa1_m2_blockdrv.s\n")
        f.write(f"// CFBlockDriver entry: ${cf_addr:04X}\n")
        f.write(f"// TestMain entry:      ${testmain_addr:04X}\n")
        f.write("#pragma once\n")
        f.write(f"#define NEO1_CFFA1_M2_BLOCKDRV_ADDR (0x{cf_addr:04X}u)\n")
        f.write(f"#define NEO1_CFFA1_M2_TESTMAIN_ADDR (0x{testmain_addr:04X}u)\n")
        f.write(f"// {len(data)} bytes\n")
        f.write("static const unsigned char neo1_cffa1_m2_blockdrv[] = {\n")
        for index in range(0, len(data), 16):
            chunk = ", ".join(f"0x{byte:02X}" for byte in data[index:index + 16])
            suffix = "," if index + 16 < len(data) else ""
            f.write(f"    {chunk}{suffix}\n")
        f.write("};\n")

    print(f"Wrote {out_path} ({len(data)} bytes) CF=${cf_addr:04X} TEST=${testmain_addr:04X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
