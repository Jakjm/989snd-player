#include "common/log/log.h"

#include <cstdarg>
#include <cstdio>
#include <mutex>

#if defined(__ANDROID__)
#include <android/log.h>
#endif

namespace lg {
namespace {
std::mutex g_log_mutex;

#if defined(__ANDROID__)
constexpr const char* kLogTag = "sndplayer";

int android_priority(level l) {
  switch (l) {
    case level::trace:
    case level::debug:
      return ANDROID_LOG_DEBUG;
    case level::info:
      return ANDROID_LOG_INFO;
    case level::warn:
      return ANDROID_LOG_WARN;
    case level::error:
      return ANDROID_LOG_ERROR;
    case level::die:
      return ANDROID_LOG_FATAL;
    default:
      return ANDROID_LOG_INFO;
  }
}
#endif

const char* level_prefix(level l) {
  switch (l) {
    case level::trace:
      return "[trace]";
    case level::debug:
      return "[debug]";
    case level::info:
      return "[info]";
    case level::warn:
      return "[warn]";
    case level::error:
      return "[error]";
    case level::die:
      return "[die]";
    default:
      return "[log]";
  }
}
}  // namespace

namespace internal {
void log_message(level log_level, LogTime& /*now*/, const char* message) {
  std::lock_guard<std::mutex> lk(g_log_mutex);
#if defined(__ANDROID__)
  __android_log_print(android_priority(log_level), kLogTag, "%s", message);
#else
  std::fprintf(stderr, "%s %s\n", level_prefix(log_level), message);
  if (log_level == level::die) {
    std::fflush(stderr);
  }
#endif
}

void log_print(const char* message) {
  std::lock_guard<std::mutex> lk(g_log_mutex);
#if defined(__ANDROID__)
  __android_log_write(ANDROID_LOG_INFO, kLogTag, message);
#else
  std::fputs(message, stderr);
#endif
}

void log_vprintf(const char* format, va_list arg_list) {
  std::lock_guard<std::mutex> lk(g_log_mutex);
#if defined(__ANDROID__)
  __android_log_vprint(ANDROID_LOG_INFO, kLogTag, format, arg_list);
#else
  std::vfprintf(stderr, format, arg_list);
#endif
}
}  // namespace internal

void printstd(const char* format, va_list arg_list) {
#if defined(__ANDROID__)
  __android_log_vprint(ANDROID_LOG_INFO, kLogTag, format, arg_list);
#else
  std::vprintf(format, arg_list);
#endif
}

void set_file(const std::string&, const bool, const bool, const std::string&) {}
void set_flush_level(level) {}
void set_file_level(level) {}
void set_stdout_level(level) {}
void set_max_debug_levels() {}
void disable_ansi_colors() {}
void initialize() {}
void finish() {}
}  // namespace lg
