; RUN: opt -load-pass-plugin %plugin -passes=nounwind-lto -S %s | FileCheck %s

declare void @callee() nounwind
declare i32 @__gxx_personality_v0(...)

define void @invoker() personality ptr @__gxx_personality_v0 {
entry:
  invoke fastcc void @callee()
          to label %cont unwind label %lpad

cont:
  ret void

lpad:
  %lp = landingpad { ptr, i32 }
          cleanup
  resume { ptr, i32 } %lp
}

; CHECK-LABEL: define void @invoker()
; CHECK: call fastcc void @callee()
; CHECK-NEXT: br label %cont
; CHECK-NOT: invoke
