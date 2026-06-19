#pragma once

#include "HealthRecord.h"
#include <string>

namespace health {

/// @brief 病历摘要记录（用户手动录入的自由文本）
///
/// 用于存储既往病史、手术史、过敏史、用药史、家族史详情等。
/// 只在 CRUD + AI 个性化分析时使用，不参与趋势计算。
struct MedicalHistoryRecord : public HealthRecord {
    /// @brief 分类标签："既往病史" / "手术史" / "过敏史" / "家族史" / "用药史" / "其他"
    std::string category;

    /// @brief 自由文本内容（用户键入）
    std::string content;
};

} // namespace health