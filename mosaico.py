#!/usr/bin/env python3
"""ESP-Mosaico product command line entry point."""

from __future__ import annotations

from pathlib import Path
import sys


REPOSITORY_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))

from mosaico_cli.cli import main  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(main())
