if(NOT DEFINED IAC_SKIP_REASON)
  set(IAC_SKIP_REASON "required test capability is unavailable")
endif()

message("IAC test skipped: ${IAC_SKIP_REASON}")
