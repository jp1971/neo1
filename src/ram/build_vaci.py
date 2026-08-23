from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from gen_vaci_header import render_header


RAM_DIR = Path(__file__).resolve().parent
SOURCE = RAM_DIR / "neo1_vaci_v1.s"
CONFIG = RAM_DIR / "neo1_vaci_v1.cfg"
HEADER = RAM_DIR / "neo1_vaci_v1.h"


def require_tool(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise RuntimeError(f"{name} was not found; install the cc65 toolchain")
    return path


def assemble() -> bytes:
    ca65 = require_tool("ca65")
    ld65 = require_tool("ld65")
    with tempfile.TemporaryDirectory(prefix="neo1-vaci-") as temp_dir:
        temp = Path(temp_dir)
        object_path = temp / "neo1_vaci_v1.o"
        binary_path = temp / "neo1_vaci_v1.bin"
        subprocess.run([ca65, str(SOURCE), "-o", str(object_path)], check=True)
        subprocess.run(
            [ld65, "-C", str(CONFIG), "-o", str(binary_path), str(object_path)],
            check=True,
        )
        return binary_path.read_bytes()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Assemble VACI and verify or update its checked-in C header"
    )
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--check", action="store_true", help="verify the header")
    action.add_argument("--update", action="store_true", help="replace the header")
    args = parser.parse_args()

    try:
        data = assemble()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    expected = render_header(data)
    if args.update:
        HEADER.write_text(expected, encoding="utf-8")
        print(f"Updated {HEADER} ({len(data)} bytes)")
        return 0

    actual = HEADER.read_text(encoding="utf-8")
    if actual != expected:
        print(
            f"error: {HEADER} is stale; run "
            f"'{sys.executable} {Path(__file__).relative_to(RAM_DIR.parent.parent)} --update'",
            file=sys.stderr,
        )
        return 1

    print(f"Verified {HEADER} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
