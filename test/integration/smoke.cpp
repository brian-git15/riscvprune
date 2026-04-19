// Side effect in callee so -O1 cannot delete the whole call chain (otherwise IR
// collapses to a single `main` that just `ret 0`).

static volatile int sink;

__attribute__((noinline)) static void callee() { sink = 1; }

__attribute__((noinline)) static void caller() { callee(); }

int main() {
  caller();
  return 0;
}
