# Tests

Generated artifacts stay under **`test/`** (see **`test/.gitignore`**). Build the plugin from repo root: **`build/NounwindLTO.so`**.

## Unit (`test/unit/`)

```bash
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/unit/basic.ll -o test/unit/basic.out.ll
```

## Integration — **guaranteed diff** (hand IR)

Clang often emits **`nounwind` before your pass** (e.g. **`-fno-exceptions`**), so **`smoke.ll` vs `smoke.out.ll`** may differ only by **`ModuleID`**. To always see the pass change IR:

```bash
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/integration/for_diff.ll -o test/integration/for_diff.out.ll
diff -u test/integration/for_diff.ll test/integration/for_diff.out.ll
```

## Integration — **smoke.cpp** (Clang → IR)

```bash
clang++ -S -emit-llvm -O0 -fexceptions -fno-inline test/integration/smoke.cpp -o test/integration/smoke.ll
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/integration/smoke.ll -o test/integration/smoke.out.ll
```

**Note:** **`clang++ -fpass-plugin=…`** loads the plugin but may not schedule **`nounwind-lto`**; use **`opt`** as above to be sure.
