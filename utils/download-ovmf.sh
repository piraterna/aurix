#!/bin/bash

check_command() {
    if ! command -v "$1" &> /dev/null; then
        echo "[!] '$1' could not be found!"
        exit 1
    fi
}

check_command curl

mkdir -p ovmf

curl -s https://retrage.github.io/edk2-nightly/bin/RELEASEX64_OVMF.fd -o ovmf/ovmf-x86_64.fd
curl -s https://retrage.github.io/edk2-nightly/bin/RELEASEX64_OVMF_VARS.fd -o ovmf/ovmf-x86_64-vars.fd
curl -s https://retrage.github.io/edk2-nightly/bin/RELEASEIa32_OVMF.fd -o ovmf/ovmf-i686.fd
curl -s https://retrage.github.io/edk2-nightly/bin/RELEASEIa32_OVMF_VARS.fd -o ovmf/ovmf-i686-vars.fd
curl -s https://retrage.github.io/edk2-nightly/bin/RELEASEAARCH64_QEMU_EFI.fd -o ovmf/ovmf-aarch64.fd
curl -s https://retrage.github.io/edk2-nightly/bin/RELEASEAARCH64_QEMU_VARS.fd -o ovmf/ovmf-aarch64-vars.fd
curl -s https://retrage.github.io/edk2-nightly/bin/RELEASERISCV64_VIRT_CODE.fd -o ovmf/ovmf-riscv64.fd
curl -s https://retrage.github.io/edk2-nightly/bin/RELEASERISCV64_VIRT_VARS.fd -o ovmf/ovmf-riscv64-vars.fd

printf " done.\n"