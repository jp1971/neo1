from pathlib import Path
import argparse


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate neo1_vaci_v1.h from neo1_vaci_v1.bin")
    parser.add_argument("--bin", required=True, dest="bin_path")
    parser.add_argument("--out", required=True, dest="out_path")
    args = parser.parse_args()

    bin_path = Path(args.bin_path)
    out_path = Path(args.out_path)
    data = bin_path.read_bytes()

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        f.write("// neo1_vaci_v1.h\n")
        f.write("// Auto-generated from neo1_vaci_v1.bin\n")
        f.write("// Do not edit manually; edit neo1_vaci_v1.s or generator inputs instead.\n")
        f.write("#pragma once\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write("#define NEO1_VACI_V1_ADDR (0xC100u)\n\n")
        f.write("// VACI V1: Neo1 Virtual Apple Cassette Interface\n")
        f.write(f"// {len(data)} bytes\n")
        f.write(f"static const uint8_t neo1_vaci_v1[{len(data)}] = {{\n")
        for index in range(0, len(data), 16):
            chunk = ", ".join(f"0x{byte:02X}" for byte in data[index:index + 16])
            suffix = "," if index + 16 < len(data) else ""
            f.write(f"    {chunk}{suffix}\n")
        f.write("};\n")

    print(f"Wrote {out_path} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
