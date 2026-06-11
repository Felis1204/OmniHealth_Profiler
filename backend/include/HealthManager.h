#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>

#include "Models/HealthRecord.h"
#include "Models/VitalsRecord.h"
#include "Models/LabTestRecord.h"
#include "Models/BloodPressureRecord.h"

namespace health {

/// @brief 趋势分析结果
struct TrendResult {
    double average;
    double min;
    double max;
    double median;
    double slope;  // 趋势斜率（正=上升，负=下降）
};

/// @brief HealthManager 控制器 —— 对外统一接口
///
/// 前端只能通过本接口与后端交互。
/// 所有内部实现（SQLite 操作、LLM 请求、风险计算）均隐藏在 .cpp 中。
class HealthManager {
public:
    HealthManager() = default;
    virtual ~HealthManager() = default;

    // ---- CRUD ----

    /// @brief 添加体征记录
    virtual bool addVitalsRecord(const VitalsRecord& record) = 0;

    /// @brief 添加临床检验记录
    virtual bool addLabTestRecord(const LabTestRecord& record) = 0;

    /// @brief 添加血压记录
    virtual bool addBloodPressureRecord(const BloodPressureRecord& record) = 0;

    /// @brief 按时间范围获取体征记录
    /// @param from 起始时间（nullopt 表示无下限）
    /// @param to 截止时间（nullopt 表示无上限）
    virtual std::vector<VitalsRecord> getVitalsRecords(
        std::optional<TimePoint> from,
        std::optional<TimePoint> to) const = 0;

    /// @brief 按时间范围获取血压记录
    virtual std::vector<BloodPressureRecord> getBloodPressureRecords(
        std::optional<TimePoint> from,
        std::optional<TimePoint> to) const = 0;

    /// @brief 按时间范围获取临床检验记录
    virtual std::vector<LabTestRecord> getLabTestRecords(
        std::optional<TimePoint> from,
        std::optional<TimePoint> to) const = 0;

    // ---- 风险计算 ----

    /// @brief 计算 10 年 ASCVD 风险评分（动脉粥样硬化性心血管疾病）
    virtual double calculateASCVDScore() const = 0;

    /// @brief 计算身体质量指数
    virtual double calculateBMI() const = 0;

    /// @brief 获取 BMI 分级描述
    virtual std::string getBMICategory() const = 0;

    // ---- 趋势分析 ----

    /// @brief 分析指定类型的指标趋势
    /// @param type 健康数据类型
    /// @param from 起始时间
    /// @param to 截止时间
    virtual TrendResult analyzeTrend(HealthRecordType type,
        TimePoint from,
        TimePoint to) const = 0;

    // ---- LLM 咨询 ----

    /// @brief 生成健康分析报告
    virtual std::string generateHealthReport() const = 0;

    /// @brief 向 AI 健康顾问提问
    /// @param userQuery 用户问题
    virtual std::string askHealthAdvisor(const std::string& userQuery) const = 0;
};

/// @brief 工厂函数 —— 创建 HealthManager 实例
std::unique_ptr<HealthManager> createHealthManager();

} // namespace health