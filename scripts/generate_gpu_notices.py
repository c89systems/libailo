#!/usr/bin/env python3
"""Generate attribution data from Ninja input/header listings; no build control."""

import pathlib
import re
import sys


def generate(base, inputs, dependencies, output):
    base = pathlib.Path(base).resolve()
    source, binary = base / "source", base / "build"
    paths = set(pathlib.Path(inputs).read_text(encoding="utf-8").splitlines())
    with open(dependencies, encoding="utf-8") as lines:
        paths.update(line.strip() for line in lines if line.startswith("    "))
    files = set()
    for path in paths:
        path = (binary / path).resolve()
        if not (path.is_relative_to(source) or path.is_relative_to(binary)):
            continue
        if path.is_file() and path.suffix in {
            ".c", ".cpp", ".h", ".hpp", ".S", ".cl", ".glsl", ".comp", ".inc", ".py"
        }:
            files.add(path)
    comments = re.compile(r"/\*.*?\*/|//[^\n]*(?:\n//[^\n]*)*", re.DOTALL)
    python_comments = re.compile(r"^[ \t]*#[^\n]*(?:\n[ \t]*#[^\n]*)*", re.MULTILINE)
    license_text = re.compile(
        r"copyright|SPDX-License-Identifier|permission is hereby|redistribution", re.I
    )
    notices = [f"{base.name}: source notices from selected native build inputs and headers.\n"]
    count = 0
    for path in sorted(files):
        pattern = python_comments if path.suffix == ".py" else comments
        blocks = [match.group() for match in pattern.finditer(path.read_text(encoding="utf-8"))
                  if license_text.search(match.group())]
        if blocks:
            relative = path.relative_to(base)
            if relative.parts[0] == "build":
                relative = pathlib.Path("generated", *relative.parts[1:])
            notices.append(f"\n===== {base.name}/{relative.as_posix()} =====\n")
            notices.append("\n".join(blocks) + "\n")
            count += 1
    if not count:
        raise ValueError(f"No source notices found for selected {base.name} inputs")
    output = pathlib.Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("".join(notices), encoding="utf-8")
    print(f"{base.name}: preserved notices from {count} native inputs/headers")


if __name__ == "__main__":
    if len(sys.argv) != 5:
        sys.exit("Usage: generate_gpu_notices.py component-root inputs deps output")
    generate(*sys.argv[1:])
