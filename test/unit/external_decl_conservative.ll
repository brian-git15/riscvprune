; RUN: opt -load-pass-plugin %plugin -passes=nounwind-lto -S %s | FileCheck %s

declare void @external_decl()

define void @caller_external() {
entry:
  call void @external_decl()
  ret void
}

; Unknown declaration boundary stays conservative.
; CHECK: define void @caller_external() {
