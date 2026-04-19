; ModuleID = 'test/unit/basic.ll'
source_filename = "test/unit/basic.ll"

; Function Attrs: nounwind
declare void @callee() #0

; Function Attrs: nounwind
define void @caller() #0 {
entry:
  call void @callee()
  ret void
}

attributes #0 = { nounwind }
