#!/usr/bin/env bash
# Emit per-TU LLVM IR, link into one module, run nounwind-lto.
# Run from repo root (or any cwd); paths resolve relative to this script.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${HERE}/_out"
PLUGIN="${ROOT}/build/NounwindLTO.so"

CXXFLAGS=(-std=c++17 -O0 -fexceptions -fno-inline -g0
          -S -emit-llvm
          -I"${HERE}")

SOURCES=(
  rng.cpp
  prizes.cpp
  ledger.cpp
  attendant.cpp
  machine.cpp
  booth.cpp
  main.cpp
)

# Prefer unversioned names, then common Ubuntu version suffixes.
find_tool() {
  local base="$1"
  local c
  for c in "${base}" "${base}-18" "${base}-19" "${base}-20" "${base}-21" "${base}-22"; do
    if command -v "${c}" >/dev/null 2>&1; then
      command -v "${c}"
      return 0
    fi
  done
  return 1
}

if [[ ! -f "${PLUGIN}" ]]; then
  echo "missing plugin: ${PLUGIN}" >&2
  echo "build first: cmake -S . -B build -G Ninja && cmake --build build" >&2
  exit 1
fi

OPT_BIN="$(find_tool opt || true)"
if [[ -z "${OPT_BIN}" ]]; then
  echo "missing opt (need LLVM tools, e.g. llvm-18 package or ./docker-shell.sh)" >&2
  exit 1
fi

mkdir -p "${OUT}"
rm -f "${OUT}"/*.ll "${OUT}"/*.bc "${OUT}"/bundle.cpp 2>/dev/null || true

LLS=()
for src in "${SOURCES[@]}"; do
  base="${src%.cpp}"
  ll="${OUT}/${base}.ll"
  echo "clang++ -> ${ll}"
  clang++ "${CXXFLAGS[@]}" "${HERE}/${src}" -o "${ll}"
  LLS+=("${ll}")
done

LINKED="${OUT}/ticket_booth.linked.ll"
LLVM_LINK_BIN="$(find_tool llvm-link || true)"

if [[ -n "${LLVM_LINK_BIN}" ]]; then
  echo "llvm-link (${LLVM_LINK_BIN}) -> ${LINKED}"
  "${LLVM_LINK_BIN}" -S "${LLS[@]}" -o "${LINKED}"
else
  # Host clang installs (esp. Windows) often omit llvm-link. Amalgamate
  # sources into one TU so we still get a whole-program module.
  BUNDLE="${OUT}/bundle.cpp"
  {
    echo "// AUTO-GENERATED — llvm-link not found; single-TU fallback"
    for src in "${SOURCES[@]}"; do
      echo "#include \"${HERE}/${src}\""
    done
  } > "${BUNDLE}"
  echo "clang++ amalgamation (no llvm-link) -> ${LINKED}"
  clang++ "${CXXFLAGS[@]}" "${BUNDLE}" -o "${LINKED}"
fi

PASSED="${OUT}/ticket_booth.nounwind.ll"
echo "opt (${OPT_BIN}) nounwind-lto -> ${PASSED}"
"${OPT_BIN}" -load-pass-plugin="${PLUGIN}" -passes=nounwind-lto -S "${LINKED}" -o "${PASSED}"

echo
echo "linked:  ${LINKED}"
echo "passed:  ${PASSED}"
echo "diff (nounwind / invoke highlights):"
if command -v rg >/dev/null 2>&1; then
  diff -u "${LINKED}" "${PASSED}" | rg -n '^[+-].*(nounwind|invoke|call )' || true
else
  diff -u "${LINKED}" "${PASSED}" | grep -E '^[+-].*(nounwind|invoke|call )' || true
fi

echo
echo "done."
