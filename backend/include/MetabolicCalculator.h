#pragma once

#include <string>

namespace health {

// ============================================================
// 算法入参结构体
// ============================================================

/// @brief TyG 指数（甘油三酯-葡萄糖指数）输入参数
/// Ref: Dicky et al. Diabetes & Metabolic Syndrome: CRR, 2022
struct TyGParams {
    double fastingGlucose;   // mmol/L
    double triglycerides;    // mmol/L
};

/// @brief CDRS（中国糖尿病风险评分）输入参数
/// Ref: Gao et al. Diabetic Medicine, 2010
struct CDRSParams {
    int age;                 // 岁
    bool isMale;
    double waistCm;          // 厘米
    bool hasFamilyHistory;   // 父母或兄弟姐妹是否有糖尿病
};

/// @brief 通用评估结果（含评分 + 风险定性）
struct MetabolicResult {
    double score;            // 连续评分值
    std::string riskLevel;   // 风险定性描述
};

// ============================================================
// 内分泌与代谢评估算法
// ============================================================

/// @brief 代谢计算器 —— 纯算法类，无状态，不依赖数据库
///
/// 包含：
///   - TyG 指数（胰岛素抵抗筛查）
///   - CDRS（中国糖尿病风险评分，用于筛查隐匿性糖尿病）
class MetabolicCalculator {
public:
    /// @brief 计算 TyG 指数（甘油三酯-葡萄糖指数）
    ///
    /// 公式：TyG = ln( TG(mg/dL) × FPG(mg/dL) / 2 )
    ///
    /// 风险分层：
    ///   - TyG < 8.70  → 胰岛素抵抗风险低
    ///   - TyG ≥ 8.70  → 存在胰岛素抵抗，代谢综合征高危
    static MetabolicResult calculateTyGIndex(const TyGParams& params);

    /// @brief 计算 CDRS（中国糖尿病风险评分）
    ///
    /// 累加模型：TotalScore = AgeScore + WaistScore + FamilyScore
    ///
    /// 风险分层（按性别独立切点）：
    ///   - 男性 ≥ 17 或 女性 ≥ 14 → 高度疑似隐匿性糖尿病，建议 OGTT
    ///   - 否则 → 低风险
    static MetabolicResult calculateCDRS(const CDRSParams& params);
};

} // namespace health