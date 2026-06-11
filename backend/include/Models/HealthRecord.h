#pragma once

#include <string>
#include <chrono>
#include <optional>

namespace health {

/// @brief 时间点类型别名（兼容 IntelliSense）
using TimePoint = std::chrono::system_clock::time_point;

/// @brief 健康记录类型枚举（对应派生类）
enum class HealthRecordType {
    VITALS,     // 基础体征
    LAB_TEST,   // 临床检验
    BP,         // 血压
    HISTORY     // 病历摘要
};

/// @brief 所有健康记录的抽象基类
struct HealthRecord {
    std::string id;                     // UUID 唯一标识
    HealthRecordType recordType;        // 记录类型
    TimePoint timestamp;                // 采集/录入时间
    std::optional<std::string> source;  // 数据来源（手动/设备型号）
    std::optional<std::string> note;    // 备注
};

} // namespace health