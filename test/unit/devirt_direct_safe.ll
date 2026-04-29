; RUN: opt -load-pass-plugin %plugin -passes=nounwind-lto -S %s | FileCheck %s

declare void @impl() nounwind

define void @devirt_like_direct() {
entry:
  call void @impl()
  ret void
}

; This models a post-devirtualization direct edge.
; CHECK: define void @devirt_like_direct() #0
