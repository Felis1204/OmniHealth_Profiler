#pragma once

#include "HealthRecord.h"
#include <optional>

namespace health {

/// @brief 临床检验记录（血糖、血脂、尿酸）
struct LabTestRecord : public HealthRecord {
    std::optional<double> fastingGlucose;     // 空腹血糖 (mmol/L)
    std::optional<double> totalCholesterol;   // 总胆固醇 (mmol/L)
    std::optional<double> ldlC;               // 低密度脂蛋白 (mmol/L)
    std::optional<double> hdlC;               // 高密度脂蛋白 (mmol/L)
    std::optional<double> triglycerides;      // 甘油三酯 (mmol/L)
    std::optional<double> uricAcid;           // 血尿酸 (μmol/L)
};

} // namespace health