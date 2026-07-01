#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>

#include "Models/HealthRecord.h"
#include "Models/VitalsRecord.h"
#include "Models/LabTestRecord.h"
#include "Models/BloodPressureRecord.h"
#include "Models/MedicalHistoryRecord.h"
#include "Models/UserProfile.h"
#include "MetabolicCalculator.h"

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

    /// @brief 添加病历摘要记录
    virtual bool addMedicalHistoryRecord(const MedicalHistoryRecord& record) = 0;

    /// @brief 更新病历摘要记录
    virtual bool updateMedicalHistoryRecord(const MedicalHistoryRecord& record) = 0;

    /// @brief 删除病历摘要记录
    virtual bool deleteMedicalHistoryRecord(const std::string& id) = 0;

    /// @brief 获取所有病历摘要记录（按时间降序）
    virtual std::vector<MedicalHistoryRecord> getMedicalHistoryRecords() const = 0;

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

    /// @brief 计算 TyG 指数（甘油三酯-葡萄糖指数，筛查胰岛素抵抗）
    /// @return 含评分值和风险定性的评估结果
    virtual MetabolicResult calculateTyGIndex() const = 0;

    /// @brief 计算 CDRS（中国糖尿病风险评分，筛查隐匿性糖尿病）
    /// @return 含评分值和风险定性的评估结果
    virtual MetabolicResult calculateCDRS() const = 0;

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

    // ---- 健康报告 ----

    /// @brief 报告周期类型
    enum class ReportPeriod {
        WEEKLY,   // 近 7 天
        MONTHLY   // 近 30 天
    };

    /// @brief 生成健康快照（最新单点数据，兼容旧版前端）
    virtual std::string generateHealthReport() const = 0;

    /// @brief 生成周期性健康报告（基于时段内均值）
    /// @param period WEEKLY（近7天均值）或 MONTHLY（近30天均值）
    /// @return 格式化报告文本，含各指标均值 + BMI + ASCVD + 趋势概览
    virtual std::string generateHealthReport(ReportPeriod period) const = 0;

    /// @brief 生成 AI 驱动的个性化健康报告（RAG 上下文注入）
    /// @param period WEEKLY（近7天）或 MONTHLY（近30天）
    /// @return JSON（AI 成功）或纯文本（降级兜底）
    ///
    /// AI-First 策略：
    ///   1. 若 LLMService 已配置 API Key → 提取时段数据 → 组装 Prompt → 调用 AI
    ///   2. 若 AI 调用失败（网络/超时/异常）→ 自动降级为 generateHealthReport(period)
    ///   3. 若 LLMService 未配置 → 直接返回本地报告
    virtual std::string generateAIReport(ReportPeriod period) = 0;

    // ---- LLM 咨询 ----

    /// @brief 获取指定类型的统计摘要
    /// @param type 健康记录类型
    /// @return 包含 min/max/avg 等统计信息的摘要字符串
    virtual std::string getStatistics(HealthRecordType type) const = 0;

    /// @brief 向 AI 健康顾问追问（基于最近一次报告的健康数据上下文）
    /// @param userQuestion 用户的追问（如"我应该怎么降血糖？"）
    /// @return AI 回复文本
    ///
    /// 前提：必须先调用 generateAIReport() 生成报告，该方法会缓存健康数据上下文。
    /// 如果尚未生成报告或 LLM 未配置，返回提示信息。
    virtual std::string askFollowUp(const std::string& userQuestion) = 0;

    // ---- LLM 配置（供前端设置 API 连接）----

    /// @brief 配置 LLM API 连接（供前端设置面板调用）
    /// @param endpoint API 端点 URL（OpenAI 兼容格式，如 https://api.deepseek.com/chat/completions）
    /// @param apiKey   API 密钥（为空时尝试从环境变量 OPENAI_API_KEY 读取）
    /// @param model    模型名称（如 "deepseek-v4-pro"、"deepseek-chat"）
    /// @return 配置成功返回 true（endpoint 非空且 API Key 存在）
    ///
    /// 用法：
    ///   1. 前端在"设置 → AI 配置"对话框中让用户填写三项参数
    ///   2. 调用 configureLLM(endpoint, apiKey, model) 保存配置
    ///   3. 之后 generateAIReport() 和 askFollowUp() 即可正常使用
    virtual bool configureLLM(const std::string& endpoint,
                              const std::string& apiKey = "",
                              const std::string& model = "deepseek-v4-pro") = 0;

    /// @brief 检查 LLM 是否已配置 API Key
    /// @return true 表示已配置，可以调用 generateAIReport()
    ///
    /// 前端可在启动时或设置页面调用此方法判断 AI 功能是否可用，
    /// 从而决定是否禁用 AI 报告按钮或显示"请先配置 API"提示。
    virtual bool isLLMConfigured() const = 0;
};

/// @brief 工厂函数 —— 创建 HealthManager 实例
std::unique_ptr<HealthManager> createHealthManager();

} // namespace health