#pragma once

#include "HealthRecord.h"
#include <optional>

namespace health {

/// @brief 基础体征记录（心率、步数、睡眠、体重、身高）
struct VitalsRecord : public HealthRecord {
    std::optional<double> heartRate;   // 心率 (bpm)
    std::optional<int> steps;          // 步数
    std::optional<double> sleepHours;  // 睡眠时长 (小时)
    std::optional<double> weightKg;    // 体重 (kg)
    std::optional<double> heightCm;    // 身高 (cm, 用于 BMI 计算)
};

} // namespace health