#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>

#include "Models/HealthRecord.h"
#include "Models/VitalsRecord.h"
#include "Models/LabTestRecord.h"
#include "Models/BloodPressureRecord.h"
#include "Models/UserProfile.h"

namespace health {

/// @brief 趋势分析结果（旧版，保留向后兼容）
struct TrendResult {
    double average;
    double min;
    double max;
    double median;
    double slope;  // 趋势斜率（正=上升，负=下降）
};

/// @brief 单个数据点（时间-数值对）
struct TrendPoint {
    std::string timestamp;   // ISO 8601 字符串，如 "2024-06-15T08:00:00Z"
    double value;            // 指标数值
};

/// @brief 单指标趋势序列 —— 前端用于绘制一条折线
struct MetricTrend {
    std::string metricName;               // 指标中文名，如 "收缩压"
    std::string metricKey;                // 指标键，如 "systolic"（前端用于排序/匹配）
    std::string unit;                     // 单位，如 "mmHg"、"kg"、"mmol/L"
    std::vector<TrendPoint> dataPoints;   // 时间-数值数据点序列（按时间升序）

    // 统计摘要（图表下方展示）
    double average = 0.0;
    double min     = 0.0;
    double max     = 0.0;
    double median  = 0.0;
    double slope   = 0.0;    // 线性回归斜率（正=上升趋势，负=下降趋势）
    int    count   = 0;      // 数据点个数
};

/// @brief 完整趋势分析报告 —— 一个记录类型的所有指标趋势
struct TrendReport {
    HealthRecordType recordType;          // VITALS / LAB_TEST / BP
    std::string title;                    // 如 "体征指标趋势分析"
    std::string periodLabel;              // 如 "2024-06-01 至 2024-06-18"
    std::vector<MetricTrend> metrics;     // 每个可量化指标一条折线

    /// @brief 是否包含有效数据
    bool isEmpty() const { return metrics.empty(); }
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

    /// @brief 更新体征记录（按 id 匹配，只更新 JSON 中存在的字段）
    virtual bool updateVitalsRecord(const VitalsRecord& record) = 0;

    /// @brief 更新临床检验记录
    virtual bool updateLabTestRecord(const LabTestRecord& record) = 0;

    /// @brief 更新血压记录
    virtual bool updateBloodPressureRecord(const BloodPressureRecord& record) = 0;

    /// @brief 删除体征记录
    /// @param id 记录主键
    virtual bool deleteVitalsRecord(const std::string& id) = 0;

    /// @brief 删除临床检验记录
    virtual bool deleteLabTestRecord(const std::string& id) = 0;

    /// @brief 删除血压记录
    virtual bool deleteBloodPressureRecord(const std::string& id) = 0;

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

    /// @brief 保存/更新用户档案（单用户系统，存在则更新，不存在则插入）
    virtual bool saveUserProfile(const UserProfile& profile) = 0;

    /// @brief 获取用户档案（返回第一条记录）
    virtual std::optional<UserProfile> getUserProfile() const = 0;

    /// @brief 删除用户档案
    /// @param id 用户主键
    virtual bool deleteUserProfile(const std::string& id) = 0;

    // ---- 风险计算 ----

    /// @brief 计算 10 年 ASCVD 风险评分（动脉粥样硬化性心血管疾病）
    virtual double calculateASCVDScore() const = 0;

    /// @brief 计算身体质量指数
    virtual double calculateBMI() const = 0;

    /// @brief 获取 BMI 分级描述
    virtual std::string getBMICategory() const = 0;

    // ---- 趋势分析 ----

    /// @brief 分析指定类型的指标趋势（旧版，返回默认指标的统计值）
    /// @param type 健康数据类型
    /// @param from 起始时间
    /// @param to 截止时间
    virtual TrendResult analyzeTrend(HealthRecordType type,
        TimePoint from,
        TimePoint to) const = 0;

    /// @brief 生成趋势分析报告（新版，返回所有指标的时间序列数据点 + 统计摘要）
    /// @param type 健康记录类型（VITALS / LAB_TEST / BP）
    /// @param from 起始时间（nullopt 表示追溯到最早记录）
    /// @param to   截止时间（nullopt 表示至今）
    /// @return TrendReport，每个可量化指标包含一条折线数据 + 统计值
    virtual TrendReport analyzeTrendReport(
        HealthRecordType type,
        std::optional<TimePoint> from,
        std::optional<TimePoint> to) const = 0;

    // ---- LLM 咨询 ----

    /// @brief 生成健康分析报告
    virtual std::string generateHealthReport() const = 0;

    /// @brief 获取指定类型的统计摘要
    /// @param type 健康记录类型
    /// @return 包含 min/max/avg 等统计信息的摘要字符串
    virtual std::string getStatistics(HealthRecordType type) const = 0;

    /// @brief 向 AI 健康顾问提问
    /// @param userQuery 用户问题
    virtual std::string askHealthAdvisor(const std::string& userQuery) const = 0;
};

/// @brief 工厂函数 —— 创建 HealthManager 实例
std::unique_ptr<HealthManager> createHealthManager();

} // namespace health