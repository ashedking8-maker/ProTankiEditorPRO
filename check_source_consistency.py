#!/usr/bin/env python3
"""Convenience entry point for the authoritative tools/ preflight."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).resolve().parent / "tools" / "check_source_consistency.py"),run_name="__main__")
