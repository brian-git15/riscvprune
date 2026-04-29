# Tests

Generated artifacts stay under **`test/`** (see **`test/.gitignore`**). Build the plugin from repo root: **`build/NounwindLTO.so`**.

## Quick start (Docker first)

```bash
./docker-shell.sh
```

This opens a shell in the container at `/work/riscvprune`.

Then run:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

## Unit tests (`test/unit/`)

Run each unit IR test with the pass:

```bash
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/unit/basic.ll -o test/unit/basic.out.ll
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/unit/indirect_call_conservative.ll -o test/unit/indirect_call_conservative.out.ll
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/unit/external_decl_conservative.ll -o test/unit/external_decl_conservative.out.ll
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/unit/devirt_direct_safe.ll -o test/unit/devirt_direct_safe.out.ll
```

## Integration tests

### Guaranteed diff (handwritten IR)

Clang often emits **`nounwind` before your pass** (e.g. **`-fno-exceptions`**), so **`smoke.ll` vs `smoke.out.ll`** may differ only by **`ModuleID`**. To always see the pass change IR:

```bash
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/integration/for_diff.ll -o test/integration/for_diff.out.ll
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/integration/invoke_lowering.ll -o test/integration/invoke_lowering.out.ll
diff -u test/integration/for_diff.ll test/integration/for_diff.out.ll
```

### `smoke.cpp` (Clang -> IR)

```bash
clang++ -S -emit-llvm -O0 -fexceptions -fno-inline test/integration/smoke.cpp -o test/integration/smoke.ll
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/integration/smoke.ll -o test/integration/smoke.out.ll
```

**Note:** **`clang++ -fpass-plugin=…`** loads the plugin but may not schedule **`nounwind-lto`**; use **`opt`** as above to be sure.

## WPV assumptions (Phase 1)

- **Full bitcode chain required:** whole-program precision only holds when all
  translation units and static archives in the link are available as LLVM IR
  (for example via `-flto`).
- **Dynamic library boundaries are conservative:** calls crossing into `.so` /
  `.dll` remain unknown unless explicitly covered by embedded runtime policy or
  IR-level overrides.
- **Internalization/devirtualization dependent:** best results require running
  after LTO inlining/internalization/devirtualization so indirect and external
  edges are reduced before SCC solving.
- **FullLTO only in Phase 1:** ThinLTO is intentionally out of scope for this
  phase; this plugin does not yet implement summary-aware cross-partition
  behavior.

## FullLTO scheduling note

- The plugin keeps manual `-passes=nounwind-lto` support for tests.
- For production WPV, schedule the pass through FullLTO extension points in the
  new pass manager (registered by `lib/Plugin.cpp`) so it runs late in the LTO
  pipeline.
