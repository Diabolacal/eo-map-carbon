"""Convert a binary file to a C comma-separated hex byte list."""

from __future__ import annotations

import argparse
from pathlib import Path


def convert(input_path: Path, output_path: Path) -> None:
    data = input_path.read_bytes()
    parts = []
    for i, byte in enumerate(data):
        if i:
            parts.append(",\n" if i % 16 == 0 else ", ")
        parts.append(f"0x{byte:02x}")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("".join(parts) + "\n", encoding="ascii")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("output")
    args = parser.parse_args()
    convert(Path(args.input), Path(args.output))


if __name__ == "__main__":
    main()
