# riscvprune (`NounwindLTO`)

LLVM pass plugin that **infers** which functions can never unwind (throw or propagate an exception), stamps them `nounwind`, and **lowers** safe `invoke` instructions to ordinary `call` + `br`. That removes unreachable exception edges so later LLVM passes can delete dead landing pads and cleanup code.

Pipeline name: `nounwind-lto` · Artifact: `build/NounwindLTO.so`

This document explains the ideas from first principles, then walks through each piece of the analysis and transform in detail.

---

## Exception handling for newcomers

If you have never worked with exceptions at the LLVM IR level, this section is the vocabulary the rest of the README uses.

### What “throwing” means at runtime

In C++, `throw` does not return to the caller like a normal function. The language runtime starts **stack unwinding**: it walks up the call stack looking for a matching `catch`, and along the way runs **destructors / cleanup** for objects that go out of scope.

From the optimizer’s point of view, a call that might throw has **two exits**:

1. **Normal return** — control continues after the call.
2. **Unwind** — control never returns to the next instruction; it jumps into exception machinery instead.

LLVM models that with two different call forms.

### `call` vs `invoke`

| IR form | Meaning |
|---------|---------|
| `call` | One successor: the next instruction. Unwind is not modeled as a CFG edge (the function may still be allowed to throw, but this call site does not name an unwind destination). |
| `invoke` | Two successors: a **normal** basic block and an **unwind** basic block. Used inside `try` regions (and similar) so the CFG explicitly shows where control goes if the callee throws. |

Example shape (simplified):

```llvm
invoke void @maybe_throws()
        to label %cont unwind label %lpad

cont:
  ; normal path
  ret void

lpad:
  %eh = landingpad { ptr, i32 }
          cleanup
  ; catch / cleanup / resume
  resume { ptr, i32 } %eh
```

### Landing pads, personality, and `resume`

- **`landingpad`** — First instruction of the unwind destination. It is the entry to EH cleanup or catch logic for that `invoke`.
- **Personality function** (e.g. `__gxx_personality_v0`) — Runtime helper that decides whether a thrown exception matches a catch clause, runs cleanup, or continues unwinding. This project does **not** interpret personality logic; it only cares whether an unwind edge can fire.
- **`resume`** — Continues unwinding after local cleanup when this frame does not fully handle the exception.
- **`llvm.eh.typeid.for`** — Used when matching catch types against the thrown type.

A landing pad existing in a function does **not** mean that function throws. It only means: *if* something below this frame unwinds, cleanup/catch code lives here.

### The `nounwind` attribute

`nounwind` on a function or call site is a contract: **this call never unwinds**.

Once every relevant callee is `nounwind`, an `invoke`’s unwind edge is dead. Replacing that `invoke` with `call` + `br` to the normal successor is then correct, and the landing pad can become unreachable.

That is the pruning this plugin exists to do—especially at **FullLTO**, where the whole program’s call graph is visible.

### Mental model for this project

> **Analysis:** “Can this function ever take an unwind edge?”  
> **Transform:** “If not, mark it `nounwind` and delete unwind edges that can never fire.”

---

## What this plugin contains

There is one registered LLVM module pass (`nounwind-lto`). Internally it is two stages:

| Stage | Code | Role |
|-------|------|------|
| **Unwind lattice analysis** | `lib/Analysis/NounwindAnalysis.cpp` | Interprocedural abstract interpretation over the call graph |
| **Nounwind application + invoke lowering** | `lib/Transform/NounwindPass.cpp` | Stamps `nounwind` and rewrites safe `invoke` → `call` + `br` |

```
Module IR
   │
   ▼
CallGraph + SCC bottom-up solver  ──►  UnwindLattice per defined function
   │
   ▼
Stamp nounwind on Safe definitions
   │
   ▼
Lower nounwind invoke → call + br
   │
   ▼
(Later LLVM: simplifycfg / DCE can delete dead landing pads)
```

---

## Analysis 1: Unwind lattice (`computeNounwindLattice`)

**Files:** `include/Analysis/NounwindAnalysis.h`, `lib/Analysis/NounwindAnalysis.cpp`

### What it computes

For every **defined** function in the module, a value of:

| Lattice value | Meaning |
|---------------|---------|
| `Safe` | Under this model, the function cannot unwind. |
| `Unknown` | It might unwind (indirect call, unknown external, etc.). |
| `Throwing` | It definitely can unwind (EH throw-like IR or known throw runtime). |

These form a small lattice ordered by severity:

```
Safe  ⊏  Unknown  ⊏  Throwing
```

Combining two results uses **join**: `Throwing` wins over everything; otherwise `Unknown` wins over `Safe`. A function is only `Safe` if **every** relevant effect inside it is `Safe`.

### Why a lattice?

Exception effects compose through calls. If `A` calls `B` and `B` may throw, `A` may throw. Mutual recursion (`A` ↔ `B`) needs a fixed-point. The three-point lattice is the abstract domain for that.

### Algorithm overview

1. Build LLVM’s **call graph** for the module.
2. Walk **strongly connected components (SCCs)** in bottom-up order (callees’ SCCs before callers’ SCCs on the SCC DAG)—the same order classic `CallGraphSCCPass` uses.
3. For each SCC, run a **local fixed-point** over the defined functions in that SCC.
4. Record the final lattice value for each defined function.

Declarations (functions with no body) are not stored as results; they are classified on demand when a caller looks them up.

### Step A — Transfer function (one function)

`transferFunction` starts at `Safe` and walks every instruction:

1. **EH throw-like instruction?** → join `Throwing` (see below).
2. Else if it is a **call/invoke** (`CallBase`):
   - Call site already has `nounwind` → ignore (this edge cannot unwind).
   - No direct callee (indirect call / function pointer) → join `Unknown`.
   - Else → join the callee’s lattice from `lookupCallee`.

Ordinary arithmetic, loads, stores, branches, etc. do not affect the lattice.

### Step B — What counts as “throw-like”?

| Instruction | Effect on lattice |
|-------------|-------------------|
| Non-`nounwind` `invoke` | `Throwing` — the CFG explicitly allows unwind. |
| `nounwind` `invoke` | Not throw-like — unwind edge cannot fire. |
| `landingpad` | **Never** force-throws by itself (it is catch/cleanup entry). |
| `resume` | `Throwing` only if the try region may still throw (see dead-catch rule). |
| `llvm.eh.typeid.for` | Same rule as `resume`. |
| Other instructions | Not throw-like on their own. |

**Dead-catch rule:** If a function has at least one `invoke`, and **every** `invoke` is already `nounwind`, then no unwind edge can enter a landing pad. In that case `resume` / `typeid` in catch/cleanup must **not** poison the function to `Throwing`—those blocks are dead for “does this function unwind?” If a function has `resume` but **no** `invoke` at all (odd IR), the analysis stays **conservative** and treats it as may-throw.

### Step C — Looking up a callee

`lookupCallee` resolves a callee in this order:

1. Callee already has LLVM `nounwind` → `Safe`.
2. **Embedded runtime policy** by symbol name (below).
3. Callee is in the **current SCC** → use the SCC’s current fixed-point state (missing → optimistic `Safe` to bootstrap the iteration).
4. Callee is a **declaration** → policy / existing `nounwind` / else `Unknown`.
5. Callee already finished in a previous SCC → use that stored result.
6. Otherwise → `Unknown` (should not happen in true bottom-up order).

### Step D — Solving one SCC

For mutually recursive functions `A` ↔ `B`:

1. Collect defined functions in the SCC; initialize each to `Safe`.
2. Repeatedly recompute each function’s transfer result until nothing changes.
3. Publish the final values into the module-wide result map.

Because join only moves “up” the lattice (`Safe` → `Unknown` → `Throwing`), the fixed-point terminates.

### Embedded runtime policy

Many C++ EH / libc++ helpers appear as external declarations **without** `nounwind` in bitcode, yet their contracts are known for this pipeline:

| Symbol pattern | Lattice | Why |
|----------------|---------|-----|
| `__cxa_throw`, `__cxa_rethrow`, `__cxa_allocate_exception` | `Throwing` | These *are* the throw path. |
| `__cxa_begin_catch`, `__cxa_end_catch`, `__clang_call_terminate` | `Safe` | Catch bookkeeping / terminate; must not make every catcher look like a thrower. |
| Itanium `operator new` mangling prefixes `_Znwm`, `_Znam`, `_Znwj`, `_Znaj` | `Safe` | Treated as non-throwing (trap-on-OOM style) for this Phase 1 model. |

Optional manual IR stubs live in `overlay/std_overrides.ll` if you want to merge explicit `nounwind` onto new/delete decls yourself.

### Conservatism (when the analysis refuses to say `Safe`)

The analysis is intentionally cautious:

- **Indirect calls** → `Unknown` (target unknown).
- **Unknown external declarations** (not covered by policy / `nounwind`) → `Unknown`.
- **Shared-library / dynamic boundaries** → same: without IR for the callee, assume it may unwind.
- Best results after LTO **inlining**, **internalization**, and **devirtualization**, which turn opaque edges into direct ones the lattice can see.

### Why this analysis matters

Clang often cannot prove `nounwind` for a function when looking at one translation unit. At FullLTO the whole call graph is available, so this SCC lattice can prove many leaf and mid-level helpers never unwind—especially pure computation that never calls throw sinks.

---

## Analysis / transform 2: `NounwindPass` (`nounwind-lto`)

**Files:** `include/Transform/NounwindPass.h`, `lib/Transform/NounwindPass.cpp`  
**Registration:** `lib/Plugin.cpp`

This is the module pass you run. It consumes the lattice and mutates IR.

### Step 1 — Run the lattice

```text
computeNounwindLattice(M, Lat)
```

### Step 2 — Stamp `nounwind`

For each **defined** function that does not already have `nounwind`:

- If `Lat[F] == Safe`, add `Attribute::NoUnwind` to `F`.

Declarations are not rewritten here (their behavior comes from existing attrs or embedded policy during analysis).

### Step 3 — Lower safe invokes

`lowerNounwindInvokes` collects every `invoke` in the module. For each invoke whose **callee** (or call-site attribute) is `nounwind`:

1. Build an equivalent `call` (calling convention, attributes, metadata, debug loc; tail-call kind forced to none).
2. Replace uses of the invoke’s value with the call’s result.
3. Insert `br` to the invoke’s **normal** successor.
4. Erase the `invoke` — the **unwind edge disappears**.

Example (see `test/integration/invoke_lowering.ll`):

**Before**

```llvm
invoke fastcc void @callee()
        to label %cont unwind label %lpad
```

**After** (when `@callee` is `nounwind`)

```llvm
call fastcc void @callee()
br label %cont
```

The landing pad is no longer reachable from this site. Later passes can delete it if nothing else jumps there.

Stamping in step 2 matters for step 3: callees newly proven `Safe` become `nounwind` before invoke lowering looks at call sites.

### Pass registration

| Hook | Purpose |
|------|---------|
| `-passes=nounwind-lto` | Explicit / tests via `opt` |
| FullLTO late extension point | Phase 1 production target |
| Optimizer-last extension point | Also registered for non-LTO pipelines |

Prefer scheduling **late** in FullLTO so inlining/devirt have already sharpened the call graph.

---

## End-to-end example (intuition)

Imagine:

```cpp
int add(int a, int b) { return a + b; }          // never throws

int scale(int x) { return add(x, x); }           // only calls Safe code

void claw() { throw JamError{}; }                // throws

void booth() {
  try { claw(); } catch (...) { /* handle */ }   // may unwind into catch
}
```

After linking IR:

| Function | Lattice | Pass effect |
|----------|---------|-------------|
| `add` | `Safe` | Gets `nounwind` |
| `scale` | `Safe` (calls only `Safe`) | Gets `nounwind` |
| `claw` | `Throwing` (hits `__cxa_throw`) | Unchanged |
| `booth` | Not `Safe` (invokes/calls throwing path) | Keep EH CFG |

If something `invoke`d only `scale`, that invoke becomes `call` + `br` and its landing pad can die.

A larger multi-file version of this story lives under `test/integration/ticket_booth/` (safe `rng` / `prizes` / `ledger`, throwing `machine`, try/catch `booth`, virtual `attendant`).

---

## Requirements

- CMake 3.20+
- Ninja
- LLVM 18.x (see `CMakeLists.txt`)
- Compatible Clang/Clang++
- Optional: Docker (`./docker-shell.sh`)

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Plugin: `build/NounwindLTO.so`

## Run

```bash
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S input.ll -o output.ll
```

`clang++ -fpass-plugin=...` may load the plugin without scheduling `nounwind-lto`. Prefer `opt` for deterministic runs.

## Testing

See [`test/README.md`](test/README.md) for the full suite. Quick checks:

```bash
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/unit/basic.ll -o test/unit/basic.out.ll
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S test/integration/invoke_lowering.ll -o test/integration/invoke_lowering.out.ll
```

| Test | What it locks in |
|------|------------------|
| `test/unit/basic.ll` | Caller of `nounwind` callee becomes `nounwind` |
| `test/unit/indirect_call_conservative.ll` | Indirect call → no `nounwind` |
| `test/unit/external_decl_conservative.ll` | Unknown external → no `nounwind` |
| `test/unit/devirt_direct_safe.ll` | Direct edge to safe callee → stamp caller |
| `test/integration/invoke_lowering.ll` | Safe invoke → `call` + `br` |
| `test/integration/ticket_booth/` | Multi-TU safe leaves vs throw/catch |

## How we tested and what we accomplished

Testing has two layers: small handwritten IR contracts that pin specific lattice/transform behaviors, and a larger multi-file C++ program that exercises the pass the way FullLTO would — one linked module with real `try`/`catch`, cross-TU calls, and mixed safe/throwing edges.

### Layer 1 — Unit and focused IR tests

Each file under `test/unit/` is a minimal LLVM IR module run with:

```bash
opt -load-pass-plugin=./build/NounwindLTO.so -passes=nounwind-lto -S <input.ll> -o <output.ll>
```

These lock in the analysis contracts: propagate `nounwind` through direct safe calls; refuse to prove `Safe` for indirect calls and unknown externals; lower a `nounwind` `invoke` to `call` + `br`. They are the regression suite for the lattice rules described above.

### Layer 2 — Encapsulating multi-file test (`ticket_booth`)

To exercise whole-program behavior, we built a small arcade claw-machine / ticket-booth codebase under `test/integration/ticket_booth/` (7 translation units):

| TU | Intent for the analysis |
|----|-------------------------|
| `rng`, `prizes`, `ledger` | Pure leaves — should be `Safe` / `nounwind` |
| `machine` | Rare `throw JamError` — must stay throwing |
| `booth` | `try`/`catch` orchestrator — must keep real EH edges |
| `attendant` | Virtual dispatch — stay conservative unless resolved |
| `main` | Driver above the throwing session |

**Pipeline (intended path: Docker shell, where the Linux `.so` plugin and `opt`/`llvm-link` exist):**

1. Build the plugin (`cmake` + Ninja → `build/NounwindLTO.so`).
2. `clang++ -O0 -fexceptions -fno-inline -S -emit-llvm` per TU.
3. `llvm-link` all TUs into one module (`ticket_booth.linked.ll`).
4. `opt -passes=nounwind-lto` → `ticket_booth.nounwind.ll`.
5. Diff / stats on `nounwind` stamps and `invoke` → `call` lowering.

Driver: `bash test/integration/ticket_booth/run.sh` (see also that directory’s README). Host Windows LLVM often lacks `llvm-link`/`opt` and cannot load the Linux plugin; the script falls back to a single-TU amalgamation when `llvm-link` is missing, but the full pass run needs the container.

### Measured results on `ticket_booth`

On the linked multi-file module (`clang++ -O0 -fexceptions -fno-inline`, then `nounwind-lto`), the pass produced visible, sound IR changes:

| Metric | Before → After |
|--------|----------------|
| `invoke` sites | 13 → 7 (**6 lowered**, ~46%) |
| ordinary `call` sites | 40 → 46 |
| defined funcs with `nounwind` | 29 → 31 (**+2** from the pass) |
| nounwind coverage | **31 / 36** (~86%) |
| `landingpad` / `resume` | 5 / 3 unchanged |

**What the analysis proved (and the transform applied):**

- Safe leaves and IPA gaps closed: newly stamped `nounwind` on `TicketBooth::ticketsLeft` (only calls already-safe `Ledger::balance`) and `CheerfulAttendant::tipOnWin` (only calls already-safe `tierCost`).
- Once those callees were `nounwind`, invoke lowering was justified. Lowered sites included `Ledger::earn` (×2), `ticketsLeft` (×2), `tierCost`, and `CheerfulAttendant::name`.

**What the analysis correctly refused to prove:**

- Kept invokes on `ClawMachine::attemptGrab` (explicit throw), `TicketBooth::run` (real catch), ctor / `JamError` construction, `__cxa_end_catch`, and `printf` (unknown external).
- Remaining **7 invokes** are success under the lattice, not failure: those edges may still unwind.
- Unchanged landing pads / resumes mean this pass stops at attributes + invoke form; deleting now-dead EH cleanup is left to later DCE/simplifycfg.

**Takeaway:** On a multi-TU linked module, the SCC unwind analysis propagates `Safe` across translation units and erases EH edges that only targeted proven-safe callees, while staying conservative exactly where the program can still throw or call unknown code — the intended Phase 1 FullLTO behavior. Artifacts live under `test/integration/ticket_booth/_out/` (gitignored); recompute stats with `python3 test/integration/ticket_booth/_stats.py` inside Docker after a fresh `run.sh`.

## FullLTO assumptions (Phase 1)

- Whole-program precision needs a full bitcode chain (e.g. `-flto`).
- Dynamic library boundaries stay conservative unless policy or IR attrs cover them.
- Best after inlining / internalization / devirtualization.
- ThinLTO is out of scope for this phase.

## Repository layout

| Path | Contents |
|------|----------|
| `lib/Analysis/` | Unwind lattice + SCC solver |
| `lib/Transform/` | `nounwind` stamping + invoke lowering |
| `lib/Plugin.cpp` | Pass registration / extension points |
| `include/` | Public headers |
| `test/unit/` | Focused IR contracts |
| `test/integration/` | End-to-end / diff / multi-TU |
| `overlay/std_overrides.ll` | Optional manual `nounwind` on new/delete |
| `scripts/`, `docker-shell.sh` | Toolchain / container helpers |
