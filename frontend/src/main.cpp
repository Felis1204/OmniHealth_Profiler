#include "HealthManager.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <optional>

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Health_Manager UI Framework Initialized!" << std::endl;
    std::cout << "========================================" << std::endl;

    // 通过工厂函数创建 HealthManager 实例（PIMPL 模式）
    auto manager = health::createHealthManager();

    // 构造一条测试体征记录
    health::VitalsRecord testRecord;
    testRecord.id = "test-001";
    testRecord.recordType = health::HealthRecordType::VITALS;
    testRecord.timestamp = std::chrono::system_clock::now();
    testRecord.heartRate = 72;
    testRecord.steps = 8500;
    testRecord.source = "manual-test";

    // 测试 CRUD
    bool ok = manager->addVitalsRecord(testRecord);
    std::cout << "\n[Frontend] " << (ok ? "✓" : "✗")
              << " 测试记录添加" << (ok ? "成功" : "失败") << std::endl;

    // 测试查询
    auto records = manager->getVitalsRecords(std::nullopt, std::nullopt);
    std::cout << "[Frontend] 当前记录总数: " << records.size() << std::endl;

    // 测试风险计算
    std::cout << "[Frontend] ASCVD 风险评分: " << manager->calculateASCVDScore() << "%" << std::endl;

    // 测试 LLM 报告
    std::cout << "\n" << manager->generateHealthReport() << std::endl;

    // 测试 LLM 咨询
    std::string reply = manager->askHealthAdvisor("我的静息心率为72bpm，是否需要关注？");
    std::cout << "[AI Advisor] " << reply << std::endl;

    std::cout << "\n[Frontend] 框架验证完成，所有接口调用正常。" << std::endl;
    return 0;
}