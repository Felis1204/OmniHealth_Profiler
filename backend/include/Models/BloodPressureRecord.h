#pragma once

#include "HealthRecord.h"
#include <optional>

namespace health {

/// @brief 血压专项记录（收缩压/舒张压）
struct BloodPressureRecord : public HealthRecord {
    std::optional<int> systolic;   // 收缩压 (mmHg)
    std::optional<int> diastolic;  // 舒张压 (mmHg)
};

} // namespace health