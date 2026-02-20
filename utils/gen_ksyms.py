#!/usr/bin/env python3

import subprocess
import sys


def die(msg: str) -> None:
    print(msg, file=sys.stderr)
    sys.exit(1)


def c_escape_bytes(b: bytes) -> str:
    out = []
    for ch in b:
        if ch == 0:
            out.append("\\0")
            continue
        if 32 <= ch <= 126 and ch not in (34, 92):
            out.append(chr(ch))
        else:
            out.append(f"\\x{ch:02x}")
    return "".join(out)


def main() -> int:
    if len(sys.argv) != 3:
        die(f"usage: {sys.argv[0]} <kernel-elf> <out.c>")

    kernel_elf = sys.argv[1]
    out_c = sys.argv[2]

    try:
        nm_out = subprocess.check_output(["nm", "-n", kernel_elf], text=True)
    except Exception as e:
        die(f"gen_ksyms: nm failed: {e}")

    syms = []
    for line in nm_out.splitlines():
        line = line.strip()
        if not line:
            continue
        parts = line.split(maxsplit=2)
        if len(parts) != 3:
            continue
        addr_s, typ, name = parts
        if addr_s == "U":
            continue
        if typ not in ("T", "t", "W", "w"):
            continue
        try:
            addr = int(addr_s, 16)
        except ValueError:
            continue
        if name.startswith("$"):
            continue
        syms.append((addr, name))

    syms.sort(key=lambda x: x[0])

    names_blob = bytearray()
    name_offs = []
    addrs = []
    for addr, name in syms:
        addrs.append(addr)
        name_offs.append(len(names_blob))
        names_blob.extend(name.encode("ascii", errors="backslashreplace"))
        names_blob.append(0)

    with open(out_c, "w", encoding="ascii", newline="\n") as f:
        f.write("#include <stdint.h>\n")
        f.write("#include <stddef.h>\n\n")
        f.write("const uint32_t __ksym_count = %d;\n" % len(addrs))

        f.write("const uint64_t __ksym_addrs[%d] = {\n" % len(addrs))
        for a in addrs:
            f.write("\t0x%016xULL,\n" % a)
        f.write("};\n\n")

        f.write("const uint32_t __ksym_name_offs[%d] = {\n" % len(name_offs))
        for o in name_offs:
            f.write("\t%du,\n" % o)
        f.write("};\n\n")

        f.write("const char __ksym_names[] = ")
        f.write('"' + c_escape_bytes(bytes(names_blob)) + '";\n')

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
