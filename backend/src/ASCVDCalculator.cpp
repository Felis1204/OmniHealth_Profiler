#include "ASCVDCalculator.h"

#include <cmath>
#include <iostream>
#include <sstream>

namespace health {

// ============================================================
// 单位换算常量
//   存储单位: mmol/L  →  公式输入: mg/dL
//   1 mmol/L = 38.61 mg/dL  →  mmol / 0.0259 = mg/dL
// ============================================================
static constexpr double MMOL_TO_MGDL = 0.0259; // 除数

// ============================================================
// China-PAR 男性公式系数
//   来源: Yang X, et al. Circulation. 2016;134:1430-1440
//   Supplemental Table 1 — Men
// ============================================================
namespace male {
    static constexpr double S10    = 0.9707;   // 10 年基线生存率
    static constexpr double MeanXB = 140.68;   // 总体均值 Sum

    // 主效应系数
    static constexpr double COEF_LN_AGE         = 31.97;
    static constexpr double COEF_LN_SBP         = 26.15;
    static constexpr double COEF_LN_TC          =  0.62;
    static constexpr double COEF_LN_HDL         = -0.69;
    static constexpr double COEF_LN_WAIST       = -0.71;
    static constexpr double COEF_SMOKER         =  3.96;
    static constexpr double COEF_DIABETES       =  0.36;
    static constexpr double COEF_REGION         =  0.48;   // 北方=1
    static constexpr double COEF_URBAN          = -0.16;   // 城市=1
    static constexpr double COEF_FAMILY_HISTORY =  6.22;

    // 交互项系数
    static constexpr double COEF_LNAGE_LNSBP     = -5.73;
    static constexpr double COEF_LNAGE_SMOKER    = -0.94;
    static constexpr double COEF_LNAGE_FAMHIST   = -1.53;
}

// ============================================================
// China-PAR 女性公式系数
//   来源: 同上 — Supplemental Table 1 — Women
//   注：女性模型中 family_history, urban, ln(age)*smoker,
//        ln(age)*family_history 不显著，未纳入
// ============================================================
namespace female {
    static constexpr double S10    = 0.9904;   // 10 年基线生存率
    static constexpr double MeanXB = 117.26;   // 总体均值 Sum

    // 主效应系数
    static constexpr double COEF_LN_AGE   = 24.87;
    static constexpr double COEF_LN_SBP   = 19.98;
    static constexpr double COEF_LN_TC    =  0.06;
    static constexpr double COEF_LN_HDL   = -0.22;
    static constexpr double COEF_LN_WAIST =  1.48;
    static constexpr double COEF_SMOKER   =  0.49;
    static constexpr double COEF_DIABETES =  0.57;
    static constexpr double COEF_REGION   =  0.54;   // 北方=1

    // 交互项系数
    static constexpr double COEF_LNAGE_LNSBP = -4.36;
}

// ============================================================
// calculateChinaPAR
// ============================================================
double ASCVDCalculator::calculateChinaPAR(const ASCVDParams& params) {
    // ---- 0. 年龄范围校验 ----
    if (params.age < 35 || params.age > 74) {
        std::cerr << "[ASCVD] 警告: 年龄 " << params.age
                  << " 超出 China-PAR 适用范围 (35-74)，返回 0" << std::endl;
        return 0.0;
    }

    // ---- 1. 单位换算: mmol/L → mg/dL ----
    double tcMgdl  = (params.totalCholesterol > 0.0)
                     ? params.totalCholesterol / MMOL_TO_MGDL
                     : 0.0;
    double hdlMgdl = (params.hdlC > 0.0)
                     ? params.hdlC / MMOL_TO_MGDL
                     : 0.0;

    // ---- 2. 糖尿病判定（双重来源）----
    double diabetes = 0.0;
    if (params.hasDiabetes) {
        diabetes = 1.0;
        // 显式标注 — 无需额外日志
    } else if (params.fastingGlucose >= 7.0) {
        diabetes = 1.0;
        std::cerr << "[ASCVD] 注: 空腹血糖 " << params.fastingGlucose
                  << " mmol/L ≥ 7.0，自动判定为糖尿病" << std::endl;
    }

    // ---- 3. 二值变量转换 ----
    double smoker   = params.isCurrentSmoker ? 1.0 : 0.0;
    double region   = params.isNorthern      ? 1.0 : 0.0;
    double urban    = params.isUrban         ? 1.0 : 0.0;
    double famHist  = params.hasFamilyHistory ? 1.0 : 0.0;

    // ---- 4. 自然对数 ----
    double lnAge   = std::log(static_cast<double>(params.age));
    double lnSbp   = (params.systolicBP > 0.0) ? std::log(params.systolicBP) : 0.0;
    double lnTc    = (tcMgdl > 0.0)            ? std::log(tcMgdl)            : 0.0;
    double lnHdl   = (hdlMgdl > 0.0)           ? std::log(hdlMgdl)           : 0.0;
    double lnWaist = (params.waistCm > 0.0)    ? std::log(params.waistCm)    : 0.0;

    // ---- 5. 计算 Sum (IndX'B) 与 基线参数 ----
    double sum    = 0.0;
    double s10    = 0.0;
    double meanXb = 0.0;

    if (params.isMale) {
        s10    = male::S10;
        meanXb = male::MeanXB;

        sum = male::COEF_LN_AGE   * lnAge
            + male::COEF_LN_SBP   * lnSbp
            + male::COEF_LN_TC    * lnTc
            + male::COEF_LN_HDL   * lnHdl
            + male::COEF_LN_WAIST * lnWaist
            + male::COEF_SMOKER   * smoker
            + male::COEF_DIABETES * diabetes
            + male::COEF_REGION   * region
            + male::COEF_URBAN    * urban
            + male::COEF_FAMILY_HISTORY * famHist
            + male::COEF_LNAGE_LNSBP   * lnAge * lnSbp
            + male::COEF_LNAGE_SMOKER  * lnAge * smoker
            + male::COEF_LNAGE_FAMHIST * lnAge * famHist;
    } else {
        s10    = female::S10;
        meanXb = female::MeanXB;

        sum = female::COEF_LN_AGE   * lnAge
            + female::COEF_LN_SBP   * lnSbp
            + female::COEF_LN_TC    * lnTc
            + female::COEF_LN_HDL   * lnHdl
            + female::COEF_LN_WAIST * lnWaist
            + female::COEF_SMOKER   * smoker
            + female::COEF_DIABETES * diabetes
            + female::COEF_REGION   * region
            + female::COEF_LNAGE_LNSBP * lnAge * lnSbp;
    }

    // ---- 6. 风险计算 ----
    // Risk = 1 - S10^exp(Sum - MeanXB)
    double exponent = std::exp(sum - meanXb);
    double survival = std::pow(s10, exponent);
    double risk     = 1.0 - survival;

    // 转换为百分比
    double riskPercent = risk * 100.0;

    // 边界裁剪
    if (riskPercent < 0.0) riskPercent = 0.0;
    if (riskPercent > 100.0) riskPercent = 100.0;

    std::cerr << "[ASCVD] China-PAR 计算完成: "
              << "age=" << params.age
              << " sex=" << (params.isMale ? "M" : "F")
              << " SBP=" << params.systolicBP
              << " TC(mg/dL)=" << tcMgdl
              << " HDL(mg/dL)=" << hdlMgdl
              << " waist=" << params.waistCm
              << " smoker=" << smoker
              << " diabetes=" << diabetes
              << " north=" << region
              << " urban=" << urban
              << " famHist=" << famHist
              << " → Sum=" << sum
              << " Risk=" << riskPercent << "%" << std::endl;

    return riskPercent;
}

// ============================================================
// getRiskCategory — China-PAR 五级分层
// ============================================================
std::string ASCVDCalculator::getRiskCategory(double riskPercentage) {
    if (riskPercentage < 5.0) {
        return "低危 (Low Risk)";
    }
    if (riskPercentage < 7.5) {
        return "临界风险 (Borderline Risk)";
    }
    if (riskPercentage < 10.0) {
        return "中危 (Intermediate Risk)";
    }
    if (riskPercentage < 20.0) {
        return "高危 (High Risk)";
    }
    return "极高危 (Very High Risk)";
}

} // namespace health
