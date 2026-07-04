#include "SeedData.h"
#include "HealthManager.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std::chrono;

// ---- 工具：从当前时间往前推 N 天 ----
static health::TimePoint daysAgo(int n) {
    return system_clock::now() - hours(24 * n);
}

// ---- 工具：生成 UUID（低配版，仅用于种子数据）----
static std::string makeId(const std::string& prefix, int n) {
    std::ostringstream oss;
    oss << prefix << "-seed-" << n;
    return oss.str();
}

int seedSampleData(health::HealthManager* mgr) {
    if (!mgr) return 0;

    int count = 0;
    std::srand(42);  // 固定种子，数据可复现

    // ============================================================
    // 1. 用户档案：45 岁男性，有高血压趋势，北方城市，戒烟，有家族史
    // ============================================================
    health::UserProfile profile;
    profile.id = "user-seed-001";
    profile.name = "张伟";
    profile.birthDate = std::to_string(2026 - 45) + "-03-15";  // 45 岁
    profile.gender = "MALE";
    profile.smokingStatus = "FORMER";    // 已戒烟
    profile.region = "NORTH";
    profile.urbanRural = "URBAN";
    profile.familyHistoryASCVD = true;   // 父亲 58 岁心梗
    profile.hasDiabetes = false;

    if (mgr->saveUserProfile(profile)) {
        std::cout << "[Seed] 用户档案已创建: " << profile.name << std::endl;
        ++count;
    }

    // ============================================================
    // 2. 体征记录 —— 过去 30 天，每天一条（模拟规律生活）
    // ============================================================
    for (int day = 30; day >= 0; --day) {
        health::VitalsRecord v;
        v.id = makeId("v", day);
        v.recordType = health::HealthRecordType::VITALS;
        v.timestamp = daysAgo(day);

        // 心率 68-78 bpm，随机波动
        v.heartRate = 70.0 + (std::rand() % 15);
        // 步数 6000-12000，周末少、工作日多
        int baseSteps = 8000;
        if (day % 7 >= 5) baseSteps = 5500;  // 周末
        v.steps = baseSteps + (std::rand() % 4000);
        // 睡眠 6.5-8.5 小时
        v.sleepHours = 7.0 + (std::rand() % 20) / 10.0;
        // 体重 78-81 kg，整体有下降趋势（在减肥）
        v.weightKg = 80.5 - day * 0.05 + (std::rand() % 10) / 20.0;
        // 身高固定
        v.heightCm = 175.0;
        // 腰围 92-96 cm，略有超标
        v.waistCm = 94.0 + (std::rand() % 40) / 10.0;

        v.source = "Apple Watch";

        if (mgr->addVitalsRecord(v)) ++count;
    }
    std::cout << "[Seed] 体征记录: 31 条" << std::endl;

    // ============================================================
    // 3. 血压记录 —— 每天早晚各一次（共 62 条）
    // ============================================================
    for (int day = 30; day >= 0; --day) {
        for (int period = 0; period < 2; ++period) {
            health::BloodPressureRecord bp;
            bp.id = makeId("bp", day * 2 + period);
            bp.recordType = health::HealthRecordType::BP;
            // 早上 8 点，晚上 20 点
            auto tp = daysAgo(day);
            if (period == 1) {
                tp += hours(12);
            }
            bp.timestamp = tp;

            // 收缩压 128-155，舒张压 82-98 — 1 级高血压范围
            int baseSys = 135 + (std::rand() % 21);   // 135-155
            int baseDia = 85 + (std::rand() % 14);    // 85-98
            // 晚上稍高
            if (period == 1) {
                baseSys += 3;
                baseDia += 2;
            }
            bp.systolic = baseSys;
            bp.diastolic = baseDia;
            bp.source = "欧姆龙电子血压计";

            if (mgr->addBloodPressureRecord(bp)) ++count;
        }
    }
    std::cout << "[Seed] 血压记录: 62 条" << std::endl;

    // ============================================================
    // 4. 临床检验 —— 月初 + 月末各一条（对比变化）
    // ============================================================
    // 月初：指标稍差
    {
        health::LabTestRecord lab;
        lab.id = "lab-seed-1";
        lab.recordType = health::HealthRecordType::LAB_TEST;
        lab.timestamp = daysAgo(30);
        lab.fastingGlucose = 5.9;        // 偏高（接近糖尿病前驱）
        lab.totalCholesterol = 5.4;      // 边缘升高
        lab.ldlC = 3.5;                  // 偏高
        lab.hdlC = 1.0;                  // 偏低（男性<1.0 ❌）
        lab.triglycerides = 2.1;         // 高（≥1.7 ⚠️）
        lab.uricAcid = 445.0;            // 偏高（男>420）
        lab.source = "XX医院检验科";
        if (mgr->addLabTestRecord(lab)) ++count;
    }

    // 月末：稍有改善（体重减轻+饮食控制后）
    {
        health::LabTestRecord lab;
        lab.id = "lab-seed-2";
        lab.recordType = health::HealthRecordType::LAB_TEST;
        lab.timestamp = daysAgo(1);
        lab.fastingGlucose = 5.6;        // 有所下降
        lab.totalCholesterol = 5.1;      // 改善
        lab.ldlC = 3.2;                  // 改善但仍偏高
        lab.hdlC = 1.1;                  // 小幅提升
        lab.triglycerides = 1.8;         // 改善但仍偏高
        lab.uricAcid = 425.0;            // 边缘
        lab.source = "XX医院检验科";
        if (mgr->addLabTestRecord(lab)) ++count;
    }
    std::cout << "[Seed] 临床检验记录: 2 条" << std::endl;

    // ============================================================
    // 5. 病历摘要 —— 3 条
    // ============================================================
    {
        health::MedicalHistoryRecord mh;
        mh.id = "mh-seed-1";
        mh.recordType = health::HealthRecordType::HISTORY;
        mh.timestamp = daysAgo(365);
        mh.category = "既往病史";
        mh.content = "2 型糖尿病家族史（母亲），本人近 2 年体检空腹血糖处于 5.6-6.0 mmol/L 区间。"
                     "2024 年体检发现轻度脂肪肝（超声）。2025 年因头晕就诊，诊断原发性高血压 1 级。";
        if (mgr->addMedicalHistoryRecord(mh)) ++count;
    }
    {
        health::MedicalHistoryRecord mh;
        mh.id = "mh-seed-2";
        mh.recordType = health::HealthRecordType::HISTORY;
        mh.timestamp = daysAgo(180);
        mh.category = "用药史";
        mh.content = "每日口服氨氯地平 5mg qd（降压），阿托伐他汀 10mg qn（降脂）。"
                     "无药物不良反应记录。";
        if (mgr->addMedicalHistoryRecord(mh)) ++count;
    }
    {
        health::MedicalHistoryRecord mh;
        mh.id = "mh-seed-3";
        mh.recordType = health::HealthRecordType::HISTORY;
        mh.timestamp = daysAgo(90);
        mh.category = "过敏史";
        mh.content = "青霉素皮试阳性，避免使用青霉素类及头孢类抗生素。"
                     "无食物过敏史。";
        if (mgr->addMedicalHistoryRecord(mh)) ++count;
    }
    std::cout << "[Seed] 病历摘要记录: 3 条" << std::endl;

    std::cout << "[Seed] 样例数据生成完毕，总计 " << count << " 条记录。" << std::endl;
    return count;
}
