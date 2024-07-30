#ifndef CRASHPOWEROS_KLOG_H
#define CRASHPOWEROS_KLOG_H

#define CONCAT_(a, b) a##b
#define CONCAT(a, b)  CONCAT_(a, b)

#define CEND          "\033c6c6c6;"

#define COLOR_DEBUG "\033708090;"
#define COLOR_INFO  "\0337FFFD4;"
#define COLOR_WARN  "\033FFD700;"
#define COLOR_ERROR "\033ee6363;"
#define COLOR_FATAL "\033ee6363;"

#define STR_DEBUG "[" COLOR_DEBUG "Debug" CEND "] "
#define STR_INFO  "[" COLOR_INFO "Info " CEND "] "
#define STR_WARN  "[" COLOR_WARN "Warn " CEND "] "
#define STR_ERROR "[" COLOR_ERROR "Error" CEND "] "
#define STR_FATAL "[" COLOR_FATAL "Fatal" CEND "] "

const char *_log_basename_(const char *path);

#define ARG_LOGINFO_FUNC __func__, __LINE__
#define ARG_LOGINFO_FILE _log_basename_(__FILE__)
#define STR_LOGINFO_FILE "[\0337FFFD4;%s" CEND "] "
#define STR_LOGINFO_FUNC "[%s" CEND ":%d" CEND "] "

#define ARG_LOGINFO ARG_LOGINFO_FILE, ARG_LOGINFO_FUNC
#define STR_LOGINFO STR_LOGINFO_FILE STR_LOGINFO_FUNC

#define _LOG(type, fmt, ...)                                                                       \
  logkf(CONCAT(STR, type) STR_LOGINFO CONCAT(COLOR, type) fmt CEND "\n", ARG_LOGINFO,             \
         ##__VA_ARGS__)

#define printi(fmt, ...) _LOG(_INFO, fmt, ##__VA_ARGS__)
#define printw(fmt, ...) _LOG(_WARN, fmt, ##__VA_ARGS__)
#define printe(fmt, ...) _LOG(_ERROR, fmt, ##__VA_ARGS__)

#define info(fmt, ...)  printi(fmt, ##__VA_ARGS__)
#define warn(fmt, ...)  printw(fmt, ##__VA_ARGS__)
#define error(fmt, ...) printe(fmt, ##__VA_ARGS__)

#endif
