#pragma once

#include <optional>
#include <string>

namespace health {

/// @brief 用户档案（人口学信息 + ASCVD 风险评估所需字段）
///
/// 对应 SQLite user_profile 表。
/// 字段命名与 DB 列名保持一致（snake_case），
/// C++ struct 成员使用 camelCase。
struct UserProfile {
    std::string id;                               // UUID 主键
    std::string name;                             // 姓名（NOT NULL）
    std::optional<std::string> birthDate;         // 出生日期 ISO 8601（用于计算年龄）
    std::optional<std::string> gender;            // "MALE" / "FEMALE"
    std::optional<std::string> smokingStatus;     // "NEVER" / "FORMER" / "CURRENT"
    std::optional<std::string> region;            // "NORTH" / "SOUTH"（China-PAR 地域）
    std::optional<std::string> urbanRural;        // "URBAN" / "RURAL"（China-PAR 城乡）
    std::optional<bool> familyHistoryASCVD;       // 早发 ASCVD 家族史（男<55/女<65）
    std::optional<bool> hasDiabetes;              // 确诊糖尿病（显式标注，优先级高于血糖判定）
};

} // namespace health
