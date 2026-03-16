FROM debian:bookworm-slim

# FIXME: use x86_64-elf toolchain, this depends on that the host is x86_64

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
  && apt-get install -y --no-install-recommends \
  ca-certificates \
  bash \
  git \
  make \
  python3 \
  gcc \
  g++ \
  binutils \
  clang \
  clang-format \
  lld \
  llvm \
  nasm \
  mingw-w64 \
  xorriso \
  mtools \
  dosfstools \
  util-linux \
  ffmpeg \
  jq \
  cpio \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /src

CMD ["bash"]
