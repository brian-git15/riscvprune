// Clang + -fno-exceptions usually pre-stamps nounwind, so opt may show no
// semantic change. For a guaranteed diff from IR alone, use for_diff.ll.
//
// This file: clang pipeline smoke (real functions, calls preserved).

volatile int sink;

#pragma clang optimize off

__attribute__((noinline)) static void callee() { sink = 1; }

__attribute__((noinline)) static void caller() { callee(); }

int main() {
  caller();
  return 0;
}

#pragma clang optimize on
