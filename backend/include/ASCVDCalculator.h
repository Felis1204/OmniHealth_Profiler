#pragma once

#include <string>

namespace health {

/// @brief China-PAR 10 年 ASCVD 风险评估输入参数
///
/// 参数来源：
///   - age               → 计算自 user_profile.birth_date（身份证年龄）
///   - isMale            → user_profile.gender
///   - systolicBP        → blood_pressure_records.systolic（近 2-3 次均值）
///   - fastingGlucose    → lab_test_records.fasting_glucose (mmol/L)
///   - totalCholesterol  → lab_test_records.total_cholesterol (mmol/L)
///   - hdlC              → lab_test_records.hdl_c (mmol/L)
///   - waistCm           → vitals_records.waist_cm (cm)
///   - isCurrentSmoker   → user_profile.smoking_status == "CURRENT"
///   - hasDiabetes       → user_profile.has_diabetes（显式标注，优先级高）
///   - isNorthern        → user_profile.region == "NORTH"
///   - isUrban           → user_profile.urban_rural == "URBAN"
///   - hasFamilyHistory  → user_profile.family_history_ascvd
struct ASCVDParams {
    int    age               = 0;
    bool   isMale            = true;
    double systolicBP        = 0.0;   // mmHg
    double fastingGlucose    = 0.0;   // mmol/L
    double totalCholesterol  = 0.0;   // mmol/L（计算时转换为 mg/dL）
    double hdlC              = 0.0;   // mmol/L（计算时转换为 mg/dL）
    double waistCm           = 0.0;   // cm
    bool   isCurrentSmoker   = false;
    bool   hasDiabetes       = false; // 显式诊断标记
    bool   isNorthern        = true;  // 北方=1, 南方=0
    bool   isUrban           = true;  // 城市=1, 乡村=0
    bool   hasFamilyHistory  = false; // 早发 ASCVD 家族史
};

/// @brief China-PAR 10 年 ASCVD 风险评估计算器
///
/// 参考论文：Yang X, et al. Circulation. 2016;134:1430-1440
/// Predicting the 10-Year Risks of Atherosclerotic Cardiovascular Disease
/// in Chinese Population: The China-PAR Project
class ASCVDCalculator {
public:
    ASCVDCalculator() = delete;

    /// @brief 计算 China-PAR 10 年 ASCVD 风险
    /// @param params 输入参数
    /// @return 10 年风险百分比（0.0 - 100.0）
    static double calculateChinaPAR(const ASCVDParams& params);

    /// @brief 风险分层（China-PAR 五级分层）
    /// @param riskPercentage 风险百分比
    /// @return 风险等级描述
    static std::string getRiskCategory(double riskPercentage);
};

} // namespace health
