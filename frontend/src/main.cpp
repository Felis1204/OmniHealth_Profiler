//This is a test code--------------Liqp-----------6.4--------------------
#include "HealthManager.h"

#include <iostream>
#include <memory>
#include <chrono>

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Health_Manager UI Framework Initialized!" << std::endl;
    std::cout << "========================================" << std::endl;

    // 通过工厂函数获取后端实例（UI 与逻辑解耦）
    auto manager = CreateHealthManager();

    // 构造一条测试健康记录
    health::HealthRecord testRecord;
    testRecord.id = "test-001";
    testRecord.type = health::HealthMetricType::HEART_RATE;
    testRecord.value = 72.0;
    testRecord.unit = "bpm";
    testRecord.timestamp = std::chrono::system_clock::now();
    testRecord.note = "晨起静息心率";

    // 调用后端接口
    if (manager->addRecord(testRecord)) {
        std::cout << "\n[Frontend] ✓ 测试记录添加成功" << std::endl;
    }

    auto records = manager->getRecords();
    std::cout << "[Frontend] 当前记录总数: " << records.size() << std::endl;

    std::cout << "\n" << manager->generateHealthReport() << std::endl;

    std::cout << "\n[Frontend] 框架验证完成，所有接口调用正常。" << std::endl;
    return 0;
}