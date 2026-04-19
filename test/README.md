# Tests

All generated test artifacts stay under **`test/`** (see `.gitignore` there).

The plugin binary is still built at **`build/NounwindLTO.so`** (repo root). Run commands from the **repo root**.

## Unit (`test/unit/`)

```bash
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/unit/basic.ll -o test/unit/basic.out.ll
```

Compare **`test/unit/basic.ll`** vs **`test/unit/basic.out.ll`**.

## Integration (`test/integration/`)

```bash
# -O0 keeps @callee/@caller/@main in the IR; -O1 can still fold a trivial chain to one `ret` without the volatile in smoke.cpp
clang++ -S -emit-llvm -O0 -fno-exceptions test/integration/smoke.cpp -o test/integration/smoke.ll
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/integration/smoke.ll -o test/integration/smoke.out.ll
clang++ test/integration/smoke.out.ll -o test/integration/smoke
```

**Note:** `clang++ -fpass-plugin=…` loads the plugin but may not run the **`nounwind-lto`** pipeline; use **`opt -passes=nounwind-lto`** above to be sure.
