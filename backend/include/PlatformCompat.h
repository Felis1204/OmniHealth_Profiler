#pragma once

/// @file PlatformCompat.h
/// @brief 跨平台兼容层 —— 集中管理 macOS/Windows/Linux 平台差异
///
/// 所有平台相关的 #ifdef 集中于此文件，通过 inline 函数提供统一接口。
/// 使用方式：在需要平台适配的 .cpp 文件中 #include "PlatformCompat.h"

#include <ctime>

// ============================================================
// 平台与编译器检测宏
// ============================================================

// 编译器
#if defined(_MSC_VER)
    #define HM_COMPILER_MSVC 1
#elif defined(__MINGW32__) || defined(__MINGW64__)
    #define HM_COMPILER_MINGW 1
#elif defined(__clang__) || defined(__apple_build_version__)
    #define HM_COMPILER_CLANG 1
#elif defined(__GNUC__)
    #define HM_COMPILER_GCC 1
#endif

// 平台
#if defined(_WIN32)
    #define HM_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
    #define HM_PLATFORM_APPLE 1
#elif defined(__linux__)
    #define HM_PLATFORM_LINUX 1
#else
    #define HM_PLATFORM_UNKNOWN 1
#endif

// ============================================================
// 时间函数跨平台适配
// ============================================================

namespace health {
namespace platform {

/// @brief 跨平台 gmtime —— 将 time_t 转换为 UTC tm 结构
///
/// POSIX (Linux/macOS/MinGW):  调用 gmtime_r(&t, &tm)    (参数顺序: time_t*, tm*)
/// MSVC:                       调用 gmtime_s(out, in)    (参数顺序: tm*, time_t*)
///
/// @param t  输入时间 (epoch)
/// @param out 输出 UTC tm 结构
inline void gmtimeCompat(const std::time_t* t, std::tm* out) {
#if HM_COMPILER_MSVC
    gmtime_s(out, t);
#else
    gmtime_r(t, out);
#endif
}

/// @brief 跨平台 timegm —— 将 UTC tm 结构转换回 time_t（mktime 的 UTC 版本）
///
/// POSIX (Linux/macOS/MinGW):  调用 timegm(tm)           (GNU/BSD 扩展, MinGW-w64 也提供)
/// MSVC:                       调用 _mkgmtime(tm)         (VS2015+)
///
/// @note 与 std::mktime 不同，此函数将 tm 解释为 UTC 时间，不受本地时区影响
/// @param tm UTC tm 结构
/// @return time_t (epoch 秒数)
inline std::time_t timegmCompat(std::tm* tm) {
#if HM_COMPILER_MSVC
    return _mkgmtime(tm);
#else
    return timegm(tm);
#endif
}

} // namespace platform
} // namespace health