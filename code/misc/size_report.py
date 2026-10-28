#!/usr/bin/env python3
"""Size analysis utility for badger firmware builds.

Usage:
  python3 tools/size_report.py --elf build/badger.elf --map build/badger.map
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

FLASH_ADDR_MIN = 0x08000000
FLASH_ADDR_MAX = 0x09000000


@dataclass
class MapEntry:
    section: str
    address: int
    size: int
    obj: str


def run_checked(cmd: list[str]) -> str:
    try:
        return subprocess.check_output(cmd, text=True)
    except FileNotFoundError:
        print(f"error: command not found: {cmd[0]}", file=sys.stderr)
        sys.exit(2)
    except subprocess.CalledProcessError as exc:
        print(f"error: command failed: {' '.join(cmd)}", file=sys.stderr)
        print(exc.output, file=sys.stderr)
        sys.exit(exc.returncode)


def parse_size_output(elf_path: Path, size_tool: str) -> tuple[int, int, int, int]:
    output = run_checked([size_tool, str(elf_path)])
    lines = [line.strip() for line in output.splitlines() if line.strip()]
    if len(lines) < 2:
        raise RuntimeError("unexpected size output")

    cols = lines[1].split()
    text = int(cols[0])
    data = int(cols[1])
    bss = int(cols[2])
    dec = int(cols[3])
    return text, data, bss, dec


def normalize_obj_path(obj: str) -> str:
    obj = obj.strip()

    marker = "/CMakeFiles/badger.elf.dir/"
    idx = obj.find(marker)
    if idx != -1:
        rel = obj[idx + len(marker) :]
        if rel.endswith(".obj"):
            rel = rel[:-4]
        return rel

    marker = "CMakeFiles/badger.elf.dir/"
    idx = obj.find(marker)
    if idx != -1:
        rel = obj[idx + len(marker) :]
        if rel.endswith(".obj"):
            rel = rel[:-4]
        return rel

    return obj


def parse_map_entries(map_path: Path) -> list[MapEntry]:
    entries: list[MapEntry] = []
    current_section: str | None = None

    section_with_obj = re.compile(
        r"^\s*(\.[^\s]+)\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(.+?)\s*$"
    )
    addr_size_obj = re.compile(r"^\s*0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(.+?)\s*$")
    section_only = re.compile(r"^\s*(\.[^\s]+)\s*$")

    with map_path.open("r", encoding="utf-8", errors="replace") as handle:
        for raw_line in handle:
            line = raw_line.rstrip("\n")

            if not line.strip():
                continue

            if line.lstrip().startswith("*"):
                continue

            m = section_with_obj.match(line)
            if m:
                section = m.group(1)
                address = int(m.group(2), 16)
                size = int(m.group(3), 16)
                obj = m.group(4)
                current_section = section
                entries.append(MapEntry(section, address, size, obj))
                continue

            m = section_only.match(line)
            if m:
                current_section = m.group(1)
                continue

            m = addr_size_obj.match(line)
            if m and current_section is not None:
                address = int(m.group(1), 16)
                size = int(m.group(2), 16)
                obj = m.group(3)
                entries.append(MapEntry(current_section, address, size, obj))

    return entries


def is_flash_entry(entry: MapEntry) -> bool:
    return FLASH_ADDR_MIN <= entry.address < FLASH_ADDR_MAX and entry.size > 0


def parse_top_symbols(elf_path: Path, nm_tool: str, count: int) -> list[tuple[int, str]]:
    output = run_checked([nm_tool, "-S", "--size-sort", "--radix=d", str(elf_path)])
    symbols: list[tuple[int, str]] = []

    for line in output.splitlines():
        parts = line.split()
        if len(parts) < 4:
            continue
        try:
            size = int(parts[1])
        except ValueError:
            continue
        name = parts[3]
        symbols.append((size, name))

    symbols.sort(key=lambda item: item[0], reverse=True)
    return symbols[:count]


def print_table(title: str, rows: list[tuple[int, str]], limit: int) -> None:
    print(f"\n{title}")
    print("-" * len(title))
    for size, name in rows[:limit]:
        print(f"{size:8d}  {name}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Report firmware size contributors.")
    parser.add_argument("--elf", type=Path, default=Path("build/badger"), help="Path to ELF file")
    parser.add_argument("--map", type=Path, default=Path("build/badger.map"), help="Path to linker MAP file")
    parser.add_argument("--size-tool", default="arm-none-eabi-size", help="size tool executable")
    parser.add_argument("--nm-tool", default="arm-none-eabi-nm", help="nm tool executable")
    parser.add_argument("--top", type=int, default=20, help="Number of top entries to print")
    args = parser.parse_args()

    if not args.elf.exists():
        print(f"error: ELF not found: {args.elf}", file=sys.stderr)
        sys.exit(2)
    if not args.map.exists():
        print(f"error: MAP not found: {args.map}", file=sys.stderr)
        sys.exit(2)

    text, data, bss, dec = parse_size_output(args.elf, args.size_tool)
    print("Firmware totals (from size):")
    print(f"  text : {text}")
    print(f"  data : {data}")
    print(f"  bss  : {bss}")
    print(f"  dec  : {dec}")

    entries = [entry for entry in parse_map_entries(args.map) if is_flash_entry(entry)]

    by_obj: dict[str, int] = defaultdict(int)
    by_symbol: dict[str, int] = defaultdict(int)

    for entry in entries:
        obj = normalize_obj_path(entry.obj)
        by_obj[obj] += entry.size
        by_symbol[f"{entry.section} [{obj}]"] += entry.size

    top_objects = sorted(((size, obj) for obj, size in by_obj.items()), reverse=True)
    top_sections = sorted(((size, sym) for sym, size in by_symbol.items()), reverse=True)
    top_symbols = parse_top_symbols(args.elf, args.nm_tool, args.top)

    print_table("Top source/object contributors in FLASH", top_objects, args.top)
    print_table("Top section-level contributors in FLASH", top_sections, args.top)
    print_table("Top ELF symbols by size", top_symbols, args.top)


if __name__ == "__main__":
    main()
