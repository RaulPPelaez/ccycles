
add_library(ulog ulog.c ulog.h)
target_compile_definitions(ulog PUBLIC ULOG_BUILD_COLOR=1 ULOG_BUILD_TIME=1 ULOG_BUILD_SOURCE_LOCATION=1)
