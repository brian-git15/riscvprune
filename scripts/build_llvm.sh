#!/usr/bin/env bash
set -euo pipefail

# Minimal helper to build LLVM with needed components.
# Usage: ./build_llvm.sh <llvm-src> <build-dir> [install-prefix]

if [[ $# -lt 2 ]]; then
  echo "usage: $0 <llvm-src> <build-dir> [install-prefix]"
  exit 1
fi

LLVM_SRC="$1"
BUILD_DIR="$2"
INSTALL_PREFIX="${3:-$BUILD_DIR/install}"

cmake -S "$LLVM_SRC/llvm" -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DLLVM_TARGETS_TO_BUILD="RISCV;X86" \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"

cmake --build "$BUILD_DIR" --target install -j"$(nproc)"

echo "LLVM installed to: $INSTALL_PREFIX"
