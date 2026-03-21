#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$(readlink -f "$0")")"

ROOT_DIR="$(pwd)"
SYSROOT_DIR="$(pwd)/mlibc-sysroot"
TOOLCHAIN_DIR="$(pwd)/toolchain"
PATCH_DIR="$(pwd)/patches"

GNU_MIRROR="https://ftp.sunet.se/mirror/gnu.org/gnu"

BINUTILS_VER="2.45.1"
BINUTILS_ARCHIVE="binutils-${BINUTILS_VER}.tar.xz"
BINUTILS_DIR="binutils-${BINUTILS_VER}"
BINUTILS_URL="${GNU_MIRROR}/binutils/${BINUTILS_ARCHIVE}"
BINUTILS_PATCH="${PATCH_DIR}/binutils.patch"

GCC_VER="15.2.0"
GCC_ARCHIVE="gcc-${GCC_VER}.tar.xz"
GCC_DIR="gcc-${GCC_VER}"
GCC_URL="${GNU_MIRROR}/gcc/gcc-${GCC_VER}/${GCC_ARCHIVE}"
GCC_PATCH="${PATCH_DIR}/gcc.patch"

TARGET="x86_64-aurix"

mkdir -p "$SYSROOT_DIR" "$TOOLCHAIN_DIR"

download_if_missing() {
    local url="$1"
    local archive="$2"

    if [ -f "$archive" ]; then
        echo "[skip] Archive already exists: $archive"
    else
        echo "[get] $archive"
        wget -O "$archive" "$url"
    fi
}

extract_if_missing() {
    local archive="$1"
    local srcdir="$2"

    if [ -d "$srcdir" ]; then
        echo "[skip] Source tree already exists: $srcdir"
    else
        echo "[extract] $archive"
        tar -xf "$archive"
    fi
}

apply_patch_if_needed() {
    local root="$1"
    local patch_file="$2"
    local stamp_file="$3"

    if [ ! -f "$patch_file" ]; then
        echo "error: patch file not found: $patch_file" >&2
        exit 1
    fi

    if [ -f "$root/$stamp_file" ]; then
        echo "[skip] patch already applied: $patch_file"
        return
    fi

    echo "[patch] applying $(basename "$patch_file") in $root"
    patch -d "$root" -p1 < "$patch_file"
    touch "$root/$stamp_file"
}

# ========== Setup mlibc headers ==========
echo "==> mlibc headers"

pushd mlibc >/dev/null

if [ ! -d headers-build ]; then
    echo "[setup] meson headers-build"
    meson setup \
        --cross-file=$ROOT_DIR/aurix-cross.txt \
        --prefix=/usr \
        -Dheaders_only=true \
        headers-build
else
    echo "[skip] meson already configured: mlibc/headers-build"
fi

if [ -f "$SYSROOT_DIR/usr/include/stdlib.h" ] || [ -d "$SYSROOT_DIR/usr/include/mlibc" ]; then
    echo "[skip] mlibc headers already installed in sysroot"
else
    echo "[install] mlibc headers"
    DESTDIR="$SYSROOT_DIR" ninja -C headers-build install
fi

popd >/dev/null

# ========== Build and install binutils ==========
echo "==> binutils"

download_if_missing "$BINUTILS_URL" "$BINUTILS_ARCHIVE"
extract_if_missing "$BINUTILS_ARCHIVE" "$BINUTILS_DIR"
apply_patch_if_needed "$BINUTILS_DIR" "$BINUTILS_PATCH" ".patched-binutils"

pushd "$BINUTILS_DIR" >/dev/null

mkdir -p build
pushd build >/dev/null

if [ ! -f Makefile ]; then
    echo "[configure] binutils"
    ../configure \
        --target="$TARGET" \
        --prefix=/usr \
        --with-sysroot="$SYSROOT_DIR" \
        --disable-werror \
        --enable-default-execstack=no
else
    echo "[skip] binutils already configured"
fi

if [ -x "$TOOLCHAIN_DIR/usr/bin/${TARGET}-ld" ] && [ -x "$TOOLCHAIN_DIR/usr/bin/${TARGET}-as" ]; then
    echo "[skip] binutils already installed"
else
    echo "[build] binutils"
    make -j"$(nproc)"
    echo "[install] binutils"
    DESTDIR="$TOOLCHAIN_DIR" make install
fi

popd >/dev/null
popd >/dev/null

# ========== Build and install GCC ==========
echo "==> gcc"

download_if_missing "$GCC_URL" "$GCC_ARCHIVE"
extract_if_missing "$GCC_ARCHIVE" "$GCC_DIR"
apply_patch_if_needed "$GCC_DIR" "$GCC_PATCH" ".patched-gcc"

pushd "$GCC_DIR" >/dev/null

mkdir -p build
pushd build >/dev/null

OLD_PATH=$PATH
PATH=$OLD_PATH:$TOOLCHAIN_DIR/usr/bin

if [ ! -f Makefile ]; then
    echo "[configure] gcc"
    ../configure \
        --target="$TARGET" \
        --prefix=/usr \
        --with-sysroot="$SYSROOT_DIR" \
        --enable-languages=c,c++ \
        --enable-threads=posix \
        --disable-multilib \
        --enable-shared \
        --enable-host-shared
else
    echo "[skip] gcc already configured"
fi

if [ -x "$TOOLCHAIN_DIR/usr/bin/${TARGET}-gcc" ] && [ -f "$TOOLCHAIN_DIR/usr/lib/gcc/${TARGET}/${GCC_VER}/libgcc.a" ]; then
    echo "[skip] gcc + target libgcc already installed"
else
    echo "[build] gcc"
    make -j"$(nproc)" all-gcc all-target-libgcc
    echo "[install] gcc"
    DESTDIR="$TOOLCHAIN_DIR" make install-gcc install-target-libgcc
fi

popd >/dev/null
popd >/dev/null

echo "==> done"

# ========== Build and install mlibc ==========
echo "==> mlibc"

pushd mlibc >/dev/null

if [ ! -d build ]; then
    echo "[setup] meson build"
    meson setup \
        --cross-file="$ROOT_DIR/aurix-cross.txt" \
        --prefix=/usr \
        -Ddefault_library=static \
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