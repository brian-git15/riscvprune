# Use a stable Ubuntu base
FROM ubuntu:24.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# 1. Install Essential Build Tools and LLVM Development Packages
# llvm-18: opt, llvm-dis, etc. (llvm-18-dev alone does not put `opt` on PATH)
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    python3 \
    clang-18 \
    lld-18 \
    llvm-18 \
    llvm-18-dev \
    libclang-18-dev \
    && rm -rf /var/lib/apt/lists/*

# 2. Unversioned tool names (Ubuntu ships opt-18, not opt)
RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100 && \
    update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-18 100 && \
    update-alternatives --install /usr/bin/opt opt /usr/bin/opt-18 100

# Working directory for the mounted repo (see docker-shell.sh)
WORKDIR /work/riscvprune

# Default command: just drop into a bash shell
CMD ["/bin/bash"]