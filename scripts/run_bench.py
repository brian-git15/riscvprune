#!/usr/bin/env python3
"""Automated benchmark harness for Spike/Verilator runs.

This is an initial scaffold; wire in your toolchain commands and parsing.
"""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path


def run(cmd: list[str], cwd: Path | None = None) -> int:
    print("+", " ".join(cmd))
    return subprocess.run(cmd, cwd=cwd, check=False).returncode


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True, help="Path to ELF")
    parser.add_argument("--runner", choices=["spike", "verilator"], required=True)
    args = parser.parse_args()

    if args.runner == "spike":
        return run(["spike", str(args.binary)])

    return run(["verilator", str(args.binary)])


if __name__ == "__main__":
    raise SystemExit(main())
