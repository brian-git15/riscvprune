; RUN: opt -load-pass-plugin %plugin -passes=nounwind-lto -S %s | FileCheck %s

declare void @callee() nounwind

define void @caller() {
entry:
  call void @callee()
  ret void
}

; CHECK: define void @caller() #0
