FROM ubuntu:22.04

# install all tools once!
RUN apt update && apt install -y \
    gcc-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    qemu-system-arm \
    make \
    && rm -rf /var/lib/apt/lists/*

# set working directory
WORKDIR /kernel
