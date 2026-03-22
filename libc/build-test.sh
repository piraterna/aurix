#!/usr/bin/env bash
set -exuo pipefail

cd "$(dirname "$(readlink -f "$0")")/.."

./libc/toolchain/usr/bin/x86_64-aurix-gcc hello.c -o initrd/bin/test -ffunction-sections -fdata-sections -Wl,--gc-sections  -Wl,--strip-all
