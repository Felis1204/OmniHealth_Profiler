#pragma once

#include <string>
#include <vector>
#include "Models/VitalsRecord.h"
#include "Models/LabTestRecord.h"
#include "Models/BloodPressureRecord.h"
#include "Models/UserProfile.h"

namespace health {

/// @brief LLM 服务层 —— 封装大模型 API 调用（RAG 上下文注入）
///
/// 通过 cpp-httplib 发起 HTTPS 请求，支持 OpenAI 兼容 API（DeepSeek 等）。
/// 所有 Prompt 组装逻辑集中于此，确保数据脱敏与 JSON 输出约束。
class LLMService {
public:
    LLMService() = default;
    virtual ~LLMService() = default;

    /// @brief 配置 API 连接
    /// @param endpoint API 端点 URL（OpenAI 兼容格式）
    /// @param apiKey   API 密钥（为空时从环境变量 OPENAI_API_KEY 读取）
    /// @param model    模型名称（默认 "deepseek-chat"）
    /// @return 成功返回 true
    virtual bool configure(const std::string& endpoint,
                           const std::string& apiKey = "",
                           const std::string& model = "deepseek-v4-pro") = 0;

    /// @brief 检查是否已配置 API Key
    virtual bool isConfigured() const = 0;

    /// @brief 发起对话请求（System Prompt + User Prompt → AI 回复）
    /// @param systemPrompt 系统级人设与指令
    /// @param userMessage  用户消息（含脱敏健康数据）
    /// @return AI 回复文本（合法 JSON 或错误 JSON）
    virtual std::string chat(const std::string& systemPrompt,
                             const std::string& userMessage) = 0;

    /// @brief 组装脱敏后的健康上下文 User Prompt
    /// @param profile      用户档案（自动去除 name/id）
    /// @param vitals       时段内体征记录
    /// @param bps          时段内血压记录
    /// @param labs         全部检验记录（取最新值）
    /// @param bmi          BMI 值
    /// @param bmiCategory  BMI 分级描述
    /// @param ascvd        ASCVD 10年风险值
    /// @param ascvdCategory ASCVD 风险分层
    /// @param trendSummary 趋势概览文本
    /// @param days         数据天数（7 或 30）
    /// @param periodLabel  周期标签（"周报" / "月报"）
    /// @return 格式化的 User Prompt 字符串（含异常标记）
    static std::string buildHealthContextPrompt(
        const UserProfile& profile,
        const std::vector<VitalsRecord>& vitals,
        const std::vector<BloodPressureRecord>& bps,
        const std::vector<LabTestRecord>& labs,
        double bmi, const std::string& bmiCategory,
        double ascvd, const std::string& ascvdCategory,
        const std::string& trendSummary,
        int days,
        const std::string& periodLabel);

    /// @brief 组装 System Prompt（医疗专家人设 + 强制 JSON 输出约束 + 结尾追问引导）
    /// @param periodLabel 周期标签（"周报" / "月报"）
    /// @return System Prompt 字符串
    static std::string buildSystemPrompt(const std::string& periodLabel);

    /// @brief 组装追问 System Prompt（保持专家人设，不要求 JSON）
    /// @param healthContext 之前报告中的健康数据上下文
    /// @return System Prompt 字符串
    static std::string buildFollowUpSystemPrompt(const std::string& healthContext);

    /// @brief 组装追问 User Prompt
    /// @param userQuestion 用户追问的自然语言问题
    /// @return User Prompt 字符串
    static std::string buildFollowUpUserPrompt(const std::string& userQuestion);
};

/// @brief 工厂函数
std::unique_ptr<LLMService> createLLMService();

} // namespace health