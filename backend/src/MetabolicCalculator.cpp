#include "MetabolicCalculator.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace health {

// ============================================================
// TyG Index — 甘油三酯-葡萄糖指数
// Ref: Dicky et al. Diabetes & Metabolic Syndrome: CRR, 2022
// ============================================================

MetabolicResult MetabolicCalculator::calculateTyGIndex(const TyGParams& params) {
    // 单位换算：mmol/L → mg/dL
    double tg_mg_dl  = params.triglycerides * 88.57;
    double fpg_mg_dl = params.fastingGlucose * 18.018;

    // TyG = ln( TG(mg/dL) × FPG(mg/dL) / 2 )
    double tyg = std::log(tg_mg_dl * fpg_mg_dl / 2.0);

    MetabolicResult result;
    result.score = tyg;

    // 风险分层（切点 8.70，来自文献）
    if (tyg < 8.70) {
        result.riskLevel = "Normal (胰岛素抵抗风险低)";
    } else {
        result.riskLevel = "High Risk (存在胰岛素抵抗，代谢综合征高危)";
    }

    std::cerr << "[Metabolic] TyG Index: TG=" << params.triglycerides
              << " mmol/L, FPG=" << params.fastingGlucose
              << " mmol/L → TyG=" << std::fixed << std::setprecision(2)
              << tyg << " (" << result.riskLevel << ")" << std::endl;

    return result;
}

// ============================================================
// CDRS — 中国糖尿病风险评分
// Ref: Gao et al. Diabetic Medicine, 2010
// ============================================================

MetabolicResult MetabolicCalculator::calculateCDRS(const CDRSParams& params) {
    int totalScore = 0;

    // ---- 1. 年龄得分（修复原论文区间重叠 bug）----
    if (params.age <= 35) {
        totalScore += 1;
    } else if (params.age <= 45) {  // 36-45
        totalScore += 3;
    } else if (params.age <= 55) {  // 46-55
        totalScore += 6;
    } else if (params.age <= 64) {  // 56-64
        totalScore += 9;
    } else {                         // ≥ 65
        totalScore += 12;
    }

    // ---- 2. 家族史得分 ----
    if (params.hasFamilyHistory) {
        totalScore += 8;
    } else {
        totalScore += 1;
    }

    // ---- 3. 腰围得分（cm → 市尺，工程化处理）----
    double chi = std::round(params.waistCm / 33.333 * 10.0) / 10.0;

    if (params.isMale) {
        // 男性腰围计分
        if (chi <= 2.3) {
            totalScore += 1;
        } else if (chi >= 2.4 && chi <= 2.6) {
            totalScore += 4;
        } else if (chi >= 2.7 && chi <= 2.9) {
            totalScore += 8;
        } else {  // chi >= 3.0
            totalScore += 12;
        }
    } else {
        // 女性腰围计分
        if (chi <= 2.0) {
            totalScore += 1;
        } else if (chi >= 2.1 && chi <= 2.3) {
            totalScore += 3;
        } else if (chi >= 2.4 && chi <= 2.6) {
            totalScore += 6;
        } else {  // chi >= 2.7
            totalScore += 9;
        }
    }

    // ---- 4. 风险定性（按性别独立切点）----
    MetabolicResult result;
    result.score = static_cast<double>(totalScore);

    int cutoff = params.isMale ? 17 : 14;

    if (totalScore >= cutoff) {
        result.riskLevel = "High Risk (高度疑似隐匿性糖尿病，建议OGTT)";
    } else {
        result.riskLevel = "Low Risk";
    }

    std::cerr << "[Metabolic] CDRS: age=" << params.age
              << " sex=" << (params.isMale ? "M" : "F")
              << " waist=" << params.waistCm << "cm (" << chi << "市尺)"
              << " famHist=" << (params.hasFamilyHistory ? "Y" : "N")
              << " → Score=" << totalScore << " (cutoff=" << cutoff
              << ") " << result.riskLevel << std::endl;

    return result;
}

} // namespace health