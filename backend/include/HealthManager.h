#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <optional>

namespace health {

/// @brief 健康数据类型枚举
enum class HealthMetricType {
    STEPS,              // 步数
    HEART_RATE,         // 心率 (bpm)
    BLOOD_PRESSURE,     // 血压
    BLOOD_SUGAR,        // 血糖
    SLEEP_HOURS,        // 睡眠时长
    WEIGHT_KG,          // 体重
    BMI,                // 身体质量指数
    CUSTOM              // 自定义
};

/// @brief 单条健康记录的数据结构
struct HealthRecord {
    std::string id;                         // 唯一标识符
    HealthMetricType type;                  // 数据类型
    double value;                           // 数值
    std::string unit;                       // 单位 (如 "bpm", "kg", "steps")
    std::chrono::system_clock::time_point timestamp;  // 记录时间
    std::optional<std::string> note;        // 可选备注
};

/// @brief HealthManager 契约接口
///
/// 前端(Frontend) 只能通过本接口与后端(Backend)交互，
/// 所有业务逻辑、数据存储、模型对接均在后端实现。
class HealthManager {
public:
    HealthManager() = default;
    virtual ~HealthManager() = default;

    // ---- 基础 CRUD ----

    /// @brief 添加一条健康记录
    /// @param record 待添加的健康记录
    /// @return 成功返回 true，失败返回 false
    virtual bool addRecord(const HealthRecord& record) = 0;

    /// @brief 获取所有健康记录
    /// @return 当前全部记录的副本
    virtual std::vector<HealthRecord> getRecords() const = 0;

    /// @brief 按类型筛选记录
    /// @param type 健康数据类型
    /// @return 匹配类型的记录列表
    virtual std::vector<HealthRecord> getRecordsByType(HealthMetricType type) const = 0;

    // ---- 数据分析 ----

    /// @brief 生成健康分析报告（预留给大模型 API 的接口）
    /// @return 格式化的健康报告文本
    virtual std::string generateHealthReport() const = 0;

    /// @brief 获取指定类型的统计摘要
    /// @param type 健康数据类型
    /// @return 包含 min/max/avg 等统计信息的摘要字符串
    virtual std::string getStatistics(HealthMetricType type) const = 0;
};

} // namespace health

/// @brief 工厂函数 —— 前端通过此接口获取 HealthManager 实例
/// @return HealthManager 的唯一指针（PIMPL + 多态）
std::unique_ptr<health::HealthManager> CreateHealthManager();
