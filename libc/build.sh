#!/usr/bin/env bash
set -eo pipefail

cd "$(dirname "$(readlink -f "$0")")"

if [ -z "$ARCH" ]; then
    printf "[*] \$ARCH not set, defaulting to x86_64...\n"
    ARCH=x86_64
fi

if ! command -v $ARCH-aurix-gcc; then
    printf "[!] Aurix toolchain not found!\n"
    exit 1
fi

ROOT_DIR="$(pwd)"
SYSROOT_DIR="$(pwd)/mlibc-sysroot"
PATCH_DIR="$(pwd)/patches"

TARGET="${ARCH}-aurix"

MLIBC_CFLAGS="${MLIBC_CFLAGS:--O2 -ffunction-sections -fdata-sections}"
MLIBC_CXXFLAGS="${MLIBC_CXXFLAGS:--O2 -ffunction-sections -fdata-sections}"
MLIBC_LDFLAGS="${MLIBC_LDFLAGS:--Wl,--gc-sections}"

mkdir -p "$SYSROOT_DIR"

# ========== Build and install mlibc ==========
echo "==> mlibc"

pushd mlibc >/dev/null

if [ ! -d build ]; then
    echo "[setup] meson build"
    CFLAGS="$MLIBC_CFLAGS" \
    CXXFLAGS="$MLIBC_CXXFLAGS" \
    LDFLAGS="$MLIBC_LDFLAGS" \
    meson setup \
        --cross-file="$ROOT_DIR/aurix-cross_${ARCH}.txt" \
        --prefix=/usr \
        -Ddefault_library=both \
        -Dno_headers=true \
        build
else
    echo "[skip] meson already configured: mlibc/build"
fi

if [ -f "$SYSROOT_DIR/usr/lib/libc.a" ] || [ -f "$SYSROOT_DIR/usr/lib/libmlibc.a" ]; then
    echo "[skip] mlibc already installed in sysroot"
else
    echo "[build] mlibc"
    ninja -C build
    echo "[install] mlibc"
    DESTDIR="$SYSROOT_DIR" ninja -C build install
fi

popd >/dev/null

# ========== Clean up ==========
PATH=$OLD_PATH

echo "==> done"
