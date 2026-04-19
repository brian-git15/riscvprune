#!/bin/bash
set -euo pipefail

# Git Bash rewrites arguments that look like Unix paths; Docker then gets wrong
# bind mounts and /work/riscvprune can be empty (no CMakeLists.txt). Disable that.
export MSYS2_ARG_CONV_EXCL='*'

docker build -t riscvprune .

docker run --rm \
  -v "$(pwd):/work/riscvprune" \
  -w /work/riscvprune \
  -it riscvprune