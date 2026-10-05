#!/usr/bin/env python3
import argparse
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
files = sorted(p for d in ['src', 'include', 'tests', 'examples']
               for p in (root / d).rglob('*') if p.suffix in {'.c', '.h', '.cpp', '.hpp'})
if files:
    subprocess.run(['clang-format', '--dry-run', '--Werror'] + list(map(str, files)) if args.check
                   else ['clang-format', '-i'] + list(map(str, files)), check=True)
