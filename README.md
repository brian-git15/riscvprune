# riscvprune (`NounwindLTO`)

LLVM pass plugin that infers and applies `nounwind` at module/LTO scope, then lowers provably non-throwing `invoke` instructions to `call + br` to simplify exception handling control flow.

## Features

- Computes an interprocedural unwind lattice (`Safe`, `Unknown`, `Throwing`) over call graph SCCs.
- Marks defined functions as `nounwind` when analysis proves they cannot unwind.
- Conservatively handles unresolved/indirect calls and unknown external declarations.
- Applies embedded runtime symbol policy for common Itanium/libc++ EH helper names.
- Lowers `invoke` to `call + br` when callee/callsite is `nounwind`, enabling dead EH cleanup paths.
- Registers with the new pass manager for:
  - explicit pipeline use: `-passes=nounwind-lto`
  - FullLTO late extension point (Phase 1 target)
  - optimizer-last extension point

## Requirements

- CMake 3.20+
- Ninja
- LLVM 18.x (required by `CMakeLists.txt`)
- Clang/Clang++ toolchain compatible with your LLVM install
- Optional: Docker (for reproducible environment)

## Build

### Option A: Native build

From repo root:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Expected plugin artifact:

- `build/NounwindLTO.so`

### Option B: Docker shell

```bash
./docker-shell.sh
```

Inside the container:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

## Run the pass

Use `opt` to guarantee scheduling:

```bash
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S input.ll -o output.ll
```

Note: `clang++ -fpass-plugin=...` may load the plugin but not schedule `nounwind-lto` in all flows. Prefer `opt` for deterministic testing.

## Testing

All examples below assume the plugin was built at `build/NounwindLTO.so`.

### Unit IR tests

```bash
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/unit/basic.ll -o test/unit/basic.out.ll
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/unit/indirect_call_conservative.ll -o test/unit/indirect_call_conservative.out.ll
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/unit/external_decl_conservative.ll -o test/unit/external_decl_conservative.out.ll
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/unit/devirt_direct_safe.ll -o test/unit/devirt_direct_safe.out.ll
```

### Integration tests

Guaranteed visible IR changes:

```bash
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/integration/for_diff.ll -o test/integration/for_diff.out.ll
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/integration/invoke_lowering.ll -o test/integration/invoke_lowering.out.ll
diff -u test/integration/for_diff.ll test/integration/for_diff.out.ll
```

From C++ source:

```bash
clang++ -S -emit-llvm -O0 -fexceptions -fno-inline test/integration/smoke.cpp -o test/integration/smoke.ll
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/integration/smoke.ll -o test/integration/smoke.out.ll
```

## FullLTO assumptions (current phase)

- Whole-program precision requires a full bitcode chain (for example via `-flto`).
- Dynamic/shared-library boundaries remain conservative unless policy or IR annotations cover those calls.
- Best results occur after inlining/internalization/devirtualization reduce indirect edges.
- ThinLTO behavior is intentionally out of scope in this phase.

## Repository layout

- `lib/Analysis/` - unwind lattice and SCC solver.
- `lib/Transform/` - pass application and `invoke` lowering.
- `lib/Plugin.cpp` - pass registration and extension-point wiring.
- `include/` - pass and analysis headers.
- `test/unit/` - focused IR behavior checks.
- `test/integration/` - end-to-end and diff-oriented checks.
- `scripts/build_llvm.sh` - helper to build/install LLVM toolchain.
- `docker-shell.sh` - local source-mounted Docker development shell.

## Notes

- Plugin/pass name in pipelines: `nounwind-lto`
- CMake target/project name: `NounwindLTO`
