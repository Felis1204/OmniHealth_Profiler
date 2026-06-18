// ============================================================
// OmniHealth — AI 健康报告端到端测试
// 需要设置环境变量 OPENAI_API_KEY
// ============================================================
#include "HealthManager.h"
#include "ASCVDCalculator.h"

#include <chrono>
#include <iostream>
#include <memory>

using namespace health;

int main() {
    auto mgr = createHealthManager();

    // ---- 1. 创建用户档案 ----
    UserProfile profile;
    profile.id              = "test-001";
    profile.name            = "李明";
    profile.birthDate       = "1966-06-11";
    profile.gender          = "MALE";
    profile.smokingStatus   = "CURRENT";
    profile.region          = "NORTH";
    profile.urbanRural      = "URBAN";
    profile.familyHistoryASCVD = false;
    profile.hasDiabetes     = true;
    mgr->saveUserProfile(profile);

    // ---- 2. 添加多条血压记录（模拟一周趋势）----
    auto now = std::chrono::system_clock::now();
    for (int i = 6; i >= 0; --i) {
        BloodPressureRecord bp;
        bp.id        = "bp-" + std::to_string(i);
        bp.recordType = HealthRecordType::BP;
        bp.timestamp = now - std::chrono::hours(24 * i);
        bp.systolic  = 140 + (i * 2);   // 146 → 152 上升趋势
        bp.diastolic = 88 + i;
        mgr->addBloodPressureRecord(bp);
    }

    // ---- 3. 添加多条体征记录 ----
    for (int i = 6; i >= 0; --i) {
        VitalsRecord v;
        v.id         = "v-" + std::to_string(i);
        v.recordType = HealthRecordType::VITALS;
        v.timestamp  = now - std::chrono::hours(24 * i);
        v.heartRate  = 78 + i;
        v.steps      = 8000 - (i * 500);
        v.sleepHours = 7.0 - (i * 0.2);
        v.weightKg   = 80.0;
        v.waistCm    = 95.0;
        mgr->addVitalsRecord(v);
    }

    // ---- 4. 添加检验记录 ----
    LabTestRecord lab;
    lab.id         = "lab-001";
    lab.recordType = HealthRecordType::LAB_TEST;
    lab.timestamp  = now - std::chrono::hours(72);
    lab.fastingGlucose   = 7.5;
    lab.totalCholesterol = 5.44;
    lab.hdlC             = 1.10;
    lab.ldlC             = 3.4;
    lab.triglycerides    = 2.1;
    lab.uricAcid         = 450;
    mgr->addLabTestRecord(lab);

    // ---- 5. 先打印本地报告 ----
    std::cout << "\n========== 本地周报（对照）==========\n";
    std::cout << mgr->generateHealthReport(HealthManager::ReportPeriod::WEEKLY);

    // ---- 6. AI 报告 ----
    std::cout << "\n========== AI 周报 ==========\n";
    std::string aiReport = mgr->generateAIReport(HealthManager::ReportPeriod::WEEKLY);
    std::cout << aiReport << std::endl;

    return 0;
}
