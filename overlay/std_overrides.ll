; Manual policy overrides for known C++ runtime symbols.
; Add nounwind where you have external proof they cannot throw.

declare noalias ptr @_Znwm(i64) nounwind
declare noalias ptr @_Znam(i64) nounwind
declare noalias ptr @_Znwj(i32) nounwind
declare noalias ptr @_Znaj(i32) nounwind
declare void @_ZdlPv(ptr) nounwind
declare void @_ZdaPv(ptr) nounwind
declare void @_ZdlPvm(ptr, i64) nounwind
declare void @_ZdaPvm(ptr, i64) nounwind
