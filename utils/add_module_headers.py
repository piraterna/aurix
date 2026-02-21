#!/usr/bin/env python3

"""Add the standard AurixOS file header to .c/.h files.

This prepends the project's standard comment header to any matching source file
that does not already appear to have it.

By default, this script skips common generated/vendor directories.

Usage:
  python3 utils/add_module_headers.py --check
  python3 utils/add_module_headers.py --apply
  python3 utils/add_module_headers.py --apply --root kernel
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
from typing import Iterable


HEADER = """/*********************************************************************************/
/* Module Name:  {module_name} */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy */
/*                                                                               */
/* This source is subject to the MIT License. */
/* See License.txt in the root of this repository. */
/* All other rights reserved. */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE. */
/*********************************************************************************/
"""


DEFAULT_EXCLUDES: tuple[str, ...] = (
    ".git",
    "build",
    "release",
    "sysroot",
    "third_party",
    "vendor",
    # Vendored kconfiglib (not our code style).
    str(Path("utils") / "kconfiglib"),


    # Vendored/third-party kernel code.
    str(Path("kernel") / "ext"),
    str(Path("kernel") / "include" / "ext"),
)


def _should_exclude(path: Path, root: Path, excludes: tuple[str, ...]) -> bool:
    try:
        rel = path.resolve().relative_to(root.resolve())
    except Exception:
        rel = path
    rel_s = str(rel)
    for ex in excludes:
        if rel_s == ex or rel_s.startswith(ex + os.sep):
            return True
        parts = rel.parts
        if ex in parts:
            return True
    return False


def _looks_like_has_header(text: str) -> bool:
    head = text[:4096]
    if "Module Name:" in head and "Project:" in head:
        return True
    if "This source is subject to the MIT License." in head:
        return True
    return False


def iter_source_files(root: Path, exts: tuple[str, ...], excludes: tuple[str, ...]) -> Iterable[Path]:
    for p in root.rglob("*"):
        if not p.is_file():
            continue
        if p.suffix.lower() not in exts:
            continue
        if _should_exclude(p, root, excludes):
            continue
        yield p


def add_header_to_file(path: Path, apply: bool) -> bool:
    data = path.read_bytes()
    if not data:
        return False

    text = data.decode("utf-8", errors="replace")
    if _looks_like_has_header(text):
        return False

    module_name = path.name
    new_text = HEADER.format(module_name=module_name) + "\n" + text.lstrip("\ufeff")
    if apply:
        path.write_text(new_text, encoding="utf-8", newline="\n")
    return True


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".", help="Root directory to scan")
    ap.add_argument(
        "--ext",
        action="append",
        default=None,
        help="File extension to include (repeatable). Default: .c and .h",
    )
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true", help="List files missing header")
    mode.add_argument("--apply", action="store_true", help="Modify files in place")
    ap.add_argument(
        "--exclude",
        action="append",
        default=[],
        help="Path prefix or directory name to skip (repeatable)",
    )

    args = ap.parse_args()
    root = Path(args.root)
    exts: tuple[str, ...] = tuple(str(x) for x in (args.ext or [".c", ".h"]))
    excludes: tuple[str, ...] = DEFAULT_EXCLUDES + tuple(args.exclude)

    changed: list[Path] = []
    for p in iter_source_files(root, exts, excludes):
        if add_header_to_file(p, apply=args.apply):
            changed.append(p)

    for p in changed:
        print(str(p))

    if args.check:
        return 1 if changed else 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
