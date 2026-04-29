; RUN: opt -load-pass-plugin %plugin -passes=nounwind-lto -S %s | FileCheck %s

declare void @target()

define void @caller(ptr %fp) {
entry:
  call void %fp()
  ret void
}

; Indirect edge must stay conservative.
; CHECK: define void @caller(ptr %fp) {
