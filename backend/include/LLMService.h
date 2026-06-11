#pragma once

#include <string>
#include <vector>
#include "Models/HealthRecord.h"

namespace health {

/// @brief LLM 服务层 —— 封装大模型 API 调用
///
/// 负责 HTTP 请求构建、System Prompt 模板化、Response 解析。
/// 通过 cpp-httplib 发起 HTTPS 请求。
class LLMService {
public:
    LLMService() = default;
    virtual ~LLMService() = default;

    /// @brief 配置 LLM 连接参数
    /// @param endpoint API 端点 URL（如 https://api.openai.com/v1/chat/completions）
    /// @param apiKey API 密钥
    /// @param model 模型名称（如 gpt-4o）
    /// @return 成功返回 true
    virtual bool configure(const std::string& endpoint, const std::string& apiKey, const std::string& model) = 0;

    /// @brief 发起对话请求
    /// @param systemPrompt 系统级提示词
    /// @param userMessage 用户消息
    /// @return 模型回复文本
    virtual std::string chat(const std::string& systemPrompt, const std::string& userMessage) = 0;

    /// @brief 根据近期健康数据构建上下文 Prompt
    /// @param recentData 近期健康记录列表
    /// @return 包含用户健康摘要的 System Prompt 文本
    virtual std::string buildHealthContextPrompt(const std::vector<HealthRecord>& recentData) = 0;
};

} // namespace health