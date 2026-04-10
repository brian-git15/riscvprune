; Manual policy overrides for known C++ runtime symbols.
; Add nounwind where you have external proof they cannot throw.

declare noalias ptr @_Znwm(i64) nounwind
declare void @_ZdlPv(ptr) nounwind
