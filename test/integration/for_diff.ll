; Same shape as test/unit/basic.ll: callee already nounwind, caller picks up
; nounwind from the pass — always shows a semantic diff vs opt output.
; Use when Clang-front-end IR already has nounwind everywhere.

declare void @callee() nounwind

define void @caller() {
entry:
  call void @callee()
  ret void
}
