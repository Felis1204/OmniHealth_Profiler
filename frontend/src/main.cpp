// ============================================================
// OmniHealth — China-PAR ASCVD 端到端测试
// ============================================================
#include "HealthManager.h"
#include "ASCVDCalculator.h"

#include <chrono>
#include <iostream>
#include <memory>

using namespace health;

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  OmniHealth — China-PAR ASCVD 验证测试" << std::endl;
    std::cout << "========================================" << std::endl;

    auto mgr = createHealthManager();

    // ---- 1. 创建用户档案 ----
    UserProfile profile;
    profile.id              = "user-001";
    profile.name            = "测试用户";
    profile.birthDate       = "1966-06-11";          // 60 岁
    profile.gender          = "MALE";
    profile.smokingStatus   = "NEVER";                // 不吸烟
    profile.region          = "NORTH";                // 北方
    profile.urbanRural      = "URBAN";                // 城市
    profile.familyHistoryASCVD = false;               // 无家族史
    profile.hasDiabetes     = true;                   // 显式标注糖尿病

    bool ok = mgr->saveUserProfile(profile);
    std::cout << "[1/5] 用户档案: " << (ok ? "✓" : "✗") << std::endl;

    // ---- 2. 添加血压记录（SBP=130, 论文示例）----
    BloodPressureRecord bp;
    bp.id        = "bp-001";
    bp.recordType = HealthRecordType::BP;
    bp.timestamp = std::chrono::system_clock::now();
    bp.systolic  = 130;
    bp.diastolic = 85;

    ok = mgr->addBloodPressureRecord(bp);
    std::cout << "[2/5] 血压记录 (130/85): " << (ok ? "✓" : "✗") << std::endl;

    // ---- 3. 添加临床检验记录 ----
    // TC=210 mg/dL → 5.44 mmol/L, HDL=55 mg/dL → 1.42 mmol/L
    // 糖尿病: fasting glucose = 7.5 mmol/L (用于自动判定测试)
    LabTestRecord lab;
    lab.id         = "lab-001";
    lab.recordType = HealthRecordType::LAB_TEST;
    lab.timestamp  = std::chrono::system_clock::now();
    lab.fastingGlucose   = 7.5;       // mmol/L
    lab.totalCholesterol = 5.44;
    lab.hdlC             = 1.42;
    lab.ldlC             = 3.2;
    lab.triglycerides    = 1.5;

    ok = mgr->addLabTestRecord(lab);
    std::cout << "[3/5] 临床检验 (TC=5.44, HDL=1.42, Glu=7.5): "
              << (ok ? "✓" : "✗") << std::endl;

    // ---- 4. 添加体征记录（腰围=80cm）----
    VitalsRecord vitals;
    vitals.id         = "vitals-001";
    vitals.recordType = HealthRecordType::VITALS;
    vitals.timestamp  = std::chrono::system_clock::now();
    vitals.waistCm    = 80.0;

    ok = mgr->addVitalsRecord(vitals);
    std::cout << "[4/5] 体征记录 (腰围=80cm): " << (ok ? "✓" : "✗") << std::endl;

    // ---- 5. China-PAR ASCVD 计算 ----
    double risk = mgr->calculateASCVDScore();
    std::string category = ASCVDCalculator::getRiskCategory(risk);

    std::cout << "\n========================================" << std::endl;
    std::cout << "  China-PAR 10 年 ASCVD 风险评估结果" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  风险评分: " << risk << "%" << std::endl;
    std::cout << "  风险分层: " << category << std::endl;
    std::cout << "  论文参考: ~11.0% (高危)" << std::endl;
    std::cout << "========================================" << std::endl;

    // ---- 各分层边界值测试 ----
    std::cout << "\n[分层阈值验证]" << std::endl;
    std::cout << "  3.0% → " << ASCVDCalculator::getRiskCategory(3.0) << std::endl;
    std::cout << "  6.0% → " << ASCVDCalculator::getRiskCategory(6.0) << std::endl;
    std::cout << "  8.5% → " << ASCVDCalculator::getRiskCategory(8.5) << std::endl;
    std::cout << " 15.0% → " << ASCVDCalculator::getRiskCategory(15.0) << std::endl;
    std::cout << " 25.0% → " << ASCVDCalculator::getRiskCategory(25.0) << std::endl;

    return 0;
}
