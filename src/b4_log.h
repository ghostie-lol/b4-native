#pragma once
// Minimal logging: logcat on Android, stderr elsewhere.
#include <cstdio>
#ifdef __ANDROID__
#include <android/log.h>
#define B4_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "B4", __VA_ARGS__)
#define B4_LOGW(...) __android_log_print(ANDROID_LOG_WARN, "B4", __VA_ARGS__)
#else
#define B4_LOGI(...) (std::fprintf(stderr, "[B4 I] " __VA_ARGS__), std::fputc('\n', stderr))
#define B4_LOGW(...) (std::fprintf(stderr, "[B4 W] " __VA_ARGS__), std::fputc('\n', stderr))
#endif
