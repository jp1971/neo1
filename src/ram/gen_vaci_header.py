# Low-level binary-to-header formatter used by build_vaci.py. Direct CLI use
# trusts the supplied binary and does not assemble or validate its provenance.

import argparse
from pathlib import Path


def render_header(data: bytes) -> str:
    lines = [
        "// neo1_vaci_v1.h",
        "// Auto-generated from neo1_vaci_v1.bin",
        "// Do not edit manually; edit neo1_vaci_v1.s or generator inputs instead.",
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        "#define NEO1_VACI_V1_ADDR (0xC100u)",
        "#define NEO1_VACI_V1_ROM_PROTECT_HI_OFFSET (3u)",
        "",
        "// VACI V1: Neo1 Virtual Apple Cassette Interface",
        f"// {len(data)} bytes",
        f"static const uint8_t neo1_vaci_v1[{len(data)}] = {{",
    ]
    for index in range(0, len(data), 16):
        chunk = ", ".join(f"0x{byte:02X}" for byte in data[index:index + 16])
        suffix = "," if index + 16 < len(data) else ""
        lines.append(f"    {chunk}{suffix}")
    lines.extend(["};", ""])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate neo1_vaci_v1.h from neo1_vaci_v1.bin")
    parser.add_argument("--bin", required=True, dest="bin_path")
    parser.add_argument("--out", required=True, dest="out_path")
    args = parser.parse_args()

    bin_path = Path(args.bin_path)
    out_path = Path(args.out_path)
    data = bin_path.read_bytes()

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(render_header(data), encoding="utf-8")

    print(f"Wrote {out_path} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
