# Multi-file encapsulating integration test: arcade ticket booth.

Whimsical claw-machine / ticket-booth mini codebase used to exercise
`nounwind-lto` across several translation units (safe leaves, throwing claw,
try/catch orchestrator, virtual attendant tips).

## Layout

| File | Role |
|------|------|
| `types.hpp` | Shared enums/structs |
| `rng.cpp` | Deterministic LCG (never throws) |
| `prizes.cpp` | Catalog lookups (never throws) |
| `ledger.cpp` | Ticket wallet arithmetic (never throws) |
| `attendant.cpp` | Virtual tip dispatch (indirect/conservative) |
| `machine.cpp` | Claw grab; rare `JamError` throw |
| `booth.cpp` | Session loop with `try`/`catch` |
| `main.cpp` | Driver |

## Run

From repo root (plugin must already be built):

```bash
bash test/integration/ticket_booth/run.sh
```

Artifacts land in `test/integration/ticket_booth/_out/`:

- `*.ll` — per-TU IR
- `ticket_booth.linked.ll` — `llvm-link` of all TUs
- `ticket_booth.nounwind.ll` — after `-passes=nounwind-lto`

Inspect expected safe leaves (`rng`, `prizes`, `ledger`) for new `nounwind`,
and keep an eye on `machine` / `booth` around the throw/catch edge.
