#include "LLMService.h"
#include "PlatformCompat.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>
#include <httplib.h>

using json = nlohmann::json;

namespace health {

// ============================================================
// 异常标记工具 — 根据医学参考范围添加 ⚠️ 标记
// ============================================================

static const char* flagGlucose(double val) {
    if (val > 6.1) return " ⚠️ 偏高";
    if (val < 3.9) return " ⚠️ 偏低";
    return " ✓ 正常";
}

static const char* flagTC(double val) {
    return (val >= 5.2) ? " ⚠️ 偏高" : " ✓ 正常";
}

static const char* flagHDL(double val, bool isMale) {
    double threshold = isMale ? 1.0 : 1.3;
    return (val < threshold) ? " ⚠️ 偏低" : " ✓ 正常";
}

static const char* flagLDL(double val) {
    return (val >= 3.4) ? " ⚠️ 偏高" : " ✓ 正常";
}

static const char* flagTG(double val) {
    return (val >= 1.7) ? " ⚠️ 偏高" : " ✓ 正常";
}

static const char* flagUA(double val, bool isMale) {
    double threshold = isMale ? 420.0 : 360.0;
    return (val > threshold) ? " ⚠️ 偏高" : " ✓ 正常";
}

// ============================================================
// 脱敏工具 — 从 UserProfile 提取非身份信息
// ============================================================

static std::string translateGender(const std::optional<std::string>& g) {
    if (!g) return "未知";
    if (*g == "MALE")   return "男";
    if (*g == "FEMALE") return "女";
    return *g;
}

static std::string translateSmoking(const std::optional<std::string>& s) {
    if (!s) return "未知";
    if (*s == "NEVER")   return "从不吸烟";
    if (*s == "FORMER")  return "已戒烟";
    if (*s == "CURRENT") return "当前吸烟";
    return *s;
}

static std::string translateRegion(const std::optional<std::string>& r) {
    if (!r) return "北方";
    if (*r == "SOUTH") return "南方";
    if (*r == "NORTH") return "北方";
    return *r;
}

static std::string translateUrban(const std::optional<std::string>& u) {
    if (!u) return "城市";
    if (*u == "URBAN") return "城市";
    if (*u == "RURAL") return "农村";
    return *u;
}

// ============================================================
// PIMPL 具体实现类
// ============================================================
class LLMServiceImpl : public LLMService {
public:
    LLMServiceImpl() = default;
    ~LLMServiceImpl() override = default;

    bool configure(const std::string& endpoint,
                   const std::string& apiKey,
                   const std::string& model) override;
    bool isConfigured() const override;
    std::string chat(const std::string& systemPrompt,
                     const std::string& userMessage) override;

private:
    std::string endpoint_;
    std::string apiKey_;
    std::string model_ = "deepseek-chat";
    bool configured_ = false;

    /// @brief 发送 HTTP POST 请求，返回响应 body
    std::string sendRequest(const std::string& systemPrompt,
                            const std::string& userMessage);
};

// ============================================================
// configure
// ============================================================
bool LLMServiceImpl::configure(const std::string& endpoint,
                                const std::string& apiKey,
                                const std::string& model) {
    endpoint_ = endpoint;
    model_    = model.empty() ? "deepseek-chat" : model;

    // API Key: 参数优先，其次环境变量 OPENAI_API_KEY
    if (!apiKey.empty()) {
        apiKey_ = apiKey;
    } else {
        const char* envKey = std::getenv("OPENAI_API_KEY");
        if (envKey && envKey[0] != '\0') {
            apiKey_ = envKey;
        }
    }

    if (apiKey_.empty()) {
        std::cerr << "[LLMService] 警告: API Key 未配置，AI 报告将不可用" << std::endl;
        std::cerr << "[LLMService] 请设置环境变量 OPENAI_API_KEY 或调用 configure() 传入" << std::endl;
        configured_ = false;
        return false;
    }

    configured_ = true;
    std::cerr << "[LLMService] 配置完成: endpoint=" << endpoint_
              << " model=" << model_ << std::endl;
    return true;
}

bool LLMServiceImpl::isConfigured() const {
    return configured_;
}

// ============================================================
// sendRequest — HTTP POST to OpenAI-compatible API
// ============================================================
std::string LLMServiceImpl::sendRequest(const std::string& systemPrompt,
                                         const std::string& userMessage) {
    // 解析 endpoint URL → host + path
    std::string host;
    std::string path = "/";

    std::string url = endpoint_;
    // 去掉 https:// 前缀
    if (url.rfind("https://", 0) == 0) {
        url = url.substr(8);
    } else if (url.rfind("http://", 0) == 0) {
        url = url.substr(7);
    }

    auto slashPos = url.find('/');
    if (slashPos != std::string::npos) {
        host = url.substr(0, slashPos);
        path = url.substr(slashPos);
    } else {
        host = url;
    }

    // 创建 HTTPS 客户端
    httplib::Client cli(host);
    cli.set_connection_timeout(10);   // 连接超时 10s
    cli.set_read_timeout(60);         // 读取超时 60s（AI 推理可能需要时间）
    cli.set_write_timeout(30);

    // 组装请求体
    json requestBody;
    requestBody["model"] = model_;
    requestBody["messages"] = json::array();

    json sysMsg;
    sysMsg["role"] = "system";
    sysMsg["content"] = systemPrompt;
    requestBody["messages"].push_back(sysMsg);

    json userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userMessage;
    requestBody["messages"].push_back(userMsg);

    requestBody["temperature"] = 0.7;
    requestBody["max_tokens"] = 2048;

    std::string bodyStr = requestBody.dump();

    // 设置请求头
    httplib::Headers headers = {
        {"Content-Type", "application/json"},
        {"Authorization", "Bearer " + apiKey_}
    };

    // 发送 POST
    auto res = cli.Post(path, headers, bodyStr, "application/json");

    if (!res) {
        auto err = cli.get_openssl_verify_result();
        std::cerr << "[LLMService] HTTP 请求失败: "
                  << httplib::to_string(res.error()) << std::endl;
        if (err != 0) {
            std::cerr << "[LLMService] SSL 验证错误码: " << err << std::endl;
        }
        return R"({"error": "网络请求失败，无法连接 AI 服务。请检查网络连接。"})";
    }

    int status = res->status;
    std::string respBody = res->body;

    if (status != 200) {
        std::cerr << "[LLMService] API 返回错误 HTTP " << status
                  << ": " << respBody << std::endl;

        if (status == 401) {
            return R"({"error": "API Key 无效，请检查密钥配置。"})";
        } else if (status == 429) {
            return R"({"error": "API 请求过于频繁，请稍后重试。"})";
        } else if (status >= 500) {
            return R"({"error": "AI 服务暂时不可用，请稍后重试。"})";
        }
        return "{\"error\": \"AI 服务异常 (HTTP " + std::to_string(status) + ")\"}";
    }

    // 解析 OpenAI 格式响应
    try {
        json respJson = json::parse(respBody);
        if (respJson.contains("choices") && !respJson["choices"].empty()) {
            std::string content = respJson["choices"][0]["message"]["content"];
            return content;
        }
        std::cerr << "[LLMService] 无法解析 API 响应: " << respBody << std::endl;
        return R"({"error": "AI 返回格式异常，请重试。"})";
    } catch (const json::parse_error& e) {
        std::cerr << "[LLMService] JSON 解析失败: " << e.what() << std::endl;
        return R"({"error": "AI 返回格式异常，JSON 解析失败。"})";
    }
}

// ============================================================
// chat
// ============================================================
std::string LLMServiceImpl::chat(const std::string& systemPrompt,
                                  const std::string& userMessage) {
    if (!configured_) {
        return R"({"error": "AI 顾问未配置。请先配置 API Key。"})";
    }
    return sendRequest(systemPrompt, userMessage);
}

// ============================================================
// buildSystemPrompt — 医疗专家人设 + 强制 JSON 输出
// ============================================================
std::string LLMService::buildSystemPrompt(const std::string& periodLabel) {
    return R"(你是一位顶级的哈佛医学院心血管与代谢疾病专家，拥有 20 年临床经验。
你正在通过 OmniHealth 数字孪生系统为用户生成个性化健康)" + periodLabel + R"(。

【重要规则】
1. 你的所有建议必须严格基于下方提供的用户真实健康数据，不得臆测或编造。
2. 如果某项数据缺失（标注为"无数据"），请明确说明该项无法评估，不要猜测。
3. 使用专业但通俗易懂的中文，避免过于晦涩的医学术语。
4. 所有建议必须附有免责声明。

【强制输出格式】
你必须严格返回合法的 JSON 格式，不包含任何 markdown 代码块标记（如 ```json），不包含任何 JSON 之外的文字。
JSON 结构如下：

{
  "risk_analysis": {
    "overall": "整体风险评估（2-3句话，基于 China-PAR 风险分层和异常指标）",
    "cardiovascular": "心血管专项分析（血压、血脂、ASCVD 风险解读）",
    "metabolic": "代谢专项分析（血糖、BMI、尿酸等解读）",
    "alert_items": ["需要警惕的指标1", "需要警惕的指标2"]
  },
  "action_plan": {
    "diet": ["具体饮食建议1", "具体饮食建议2", "具体饮食建议3"],
    "exercise": ["运动类型+时长+频率建议1", "运动建议2"],
    "lifestyle": ["生活习惯改善建议1", "生活习惯改善建议2"],
    "monitoring": ["建议重点监测的指标1", "建议在家自测的指标2"]
  },
  "conclusion": "一句话核心结论（包含最关键的 1-2 个行动项）",
  "disclaimer": "⚠️ 免责声明：本报告由 AI 生成，仅供参考，不构成医疗诊断或治疗建议。如有健康疑虑，请及时咨询专业医生。"
})";
}

// ============================================================
// buildHealthContextPrompt — 脱敏数据组装 + 异常标记
// ============================================================
std::string LLMService::buildHealthContextPrompt(
    const UserProfile& profile,
    const std::vector<VitalsRecord>& vitals,
    const std::vector<BloodPressureRecord>& bps,
    const std::vector<LabTestRecord>& labs,
    double bmi, const std::string& bmiCategory,
    double ascvd, const std::string& ascvdCategory,
    const std::string& trendSummary,
    int days,
    const std::string& periodLabel)
{
    // 计算年龄
    int age = 0;
    if (profile.birthDate && !profile.birthDate->empty()) {
        std::tm tm = {};
        std::istringstream ss(*profile.birthDate);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        if (!ss.fail()) {
            auto birthTime = std::chrono::system_clock::from_time_t(
                platform::timegmCompat(&tm));
            auto now = std::chrono::system_clock::now();
            auto hours = std::chrono::duration_cast<std::chrono::hours>(
                now - birthTime).count();
            age = static_cast<int>(hours / (365.25 * 24.0));
        }
    }

    bool isMale = (profile.gender && *profile.gender == "MALE");

    // 计算时段均值
    auto avgOptDouble = [](const auto& records, auto accessor) -> std::string {
        double sum = 0.0;
        int count = 0;
        for (const auto& r : records) {
            auto val = accessor(r);
            if (val) { sum += *val; ++count; }
        }
        if (count == 0) return "无数据";
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << (sum / count);
        return oss.str();
    };

    auto avgOptInt = [](const auto& records, auto accessor) -> std::string {
        double sum = 0.0;
        int count = 0;
        for (const auto& r : records) {
            auto val = accessor(r);
            if (val) { sum += static_cast<double>(*val); ++count; }
        }
        if (count == 0) return "无数据";
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << (sum / count);
        return oss.str();
    };

    // 最新腰围
    double waist = 0.0;
    for (auto it = vitals.rbegin(); it != vitals.rend(); ++it) {
        if (it->waistCm) { waist = *it->waistCm; break; }
    }

    // 最新检验
    double glucose = 0, tc = 0, hdl = 0, ldl = 0, tg = 0, ua = 0;
    bool hasLab = false;
    if (!labs.empty()) {
        const auto& latest = labs.back();
        hasLab = true;
        if (latest.fastingGlucose)   glucose = *latest.fastingGlucose;
        if (latest.totalCholesterol) tc      = *latest.totalCholesterol;
        if (latest.hdlC)             hdl     = *latest.hdlC;
        if (latest.ldlC)             ldl     = *latest.ldlC;
        if (latest.triglycerides)    tg      = *latest.triglycerides;
        if (latest.uricAcid)         ua      = *latest.uricAcid;
    }

    // 模板填充
    auto fmtVal = [](double v) -> std::string {
        if (v <= 0.0) return "无数据";
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << v;
        return oss.str();
    };

    std::ostringstream oss;
    oss << "以下是一位用户授权你读取的本地健康数据库摘要（数据已脱敏）：\n\n";

    // 基本画像
    oss << "## 基本画像\n";
    oss << "- 年龄：" << age << " 岁\n";
    oss << "- 性别：" << translateGender(profile.gender) << "\n";
    oss << "- 吸烟状况：" << translateSmoking(profile.smokingStatus) << "\n";
    oss << "- 糖尿病："
        << ((profile.hasDiabetes && *profile.hasDiabetes) ? "是" : "否") << "\n";
    oss << "- 地域：" << translateRegion(profile.region)
        << "（" << translateUrban(profile.urbanRural) << "）\n";
    oss << "- 早发 ASCVD 家族史："
        << ((profile.familyHistoryASCVD && *profile.familyHistoryASCVD) ? "有" : "无") << "\n";

    // 体征均值
    oss << "\n## 近 " << days << " 天体徵均值（" << periodLabel << "）\n";
    oss << "- 心率：" << avgOptInt(vitals,
        [](const VitalsRecord& r) { return r.heartRate; }) << " bpm\n";
    oss << "- 日均步数：" << avgOptInt(vitals,
        [](const VitalsRecord& r) { return r.steps; }) << " 步\n";
    oss << "- 日均睡眠：" << avgOptDouble(vitals,
        [](const VitalsRecord& r) { return r.sleepHours; }) << " 小时\n";
    oss << "- 体重：" << avgOptDouble(vitals,
        [](const VitalsRecord& r) { return r.weightKg; }) << " kg\n";
    if (waist > 0)
        oss << "- 腰围（最新）：" << std::fixed << std::setprecision(0) << waist << " cm\n";

    // 血压均值
    oss << "\n## 近 " << days << " 天血压均值（共 " << bps.size() << " 次测量）\n";
    oss << "- 收缩压：" << avgOptInt(bps,
        [](const BloodPressureRecord& r) { return r.systolic; }) << " mmHg\n";
    oss << "- 舒张压：" << avgOptInt(bps,
        [](const BloodPressureRecord& r) { return r.diastolic; }) << " mmHg\n";

    // 临床检验
    if (hasLab) {
        oss << "\n## 最新临床检验\n";
        if (glucose > 0)
            oss << "- 空腹血糖：" << fmtVal(glucose)
                << " mmol/L" << flagGlucose(glucose) << "\n";
        if (tc > 0)
            oss << "- 总胆固醇：" << fmtVal(tc)
                << " mmol/L" << flagTC(tc) << "\n";
        if (hdl > 0)
            oss << "- HDL-C：" << fmtVal(hdl)
                << " mmol/L" << flagHDL(hdl, isMale) << "\n";
        if (ldl > 0)
            oss << "- LDL-C：" << fmtVal(ldl)
                << " mmol/L" << flagLDL(ldl) << "\n";
        if (tg > 0)
            oss << "- 甘油三酯：" << fmtVal(tg)
                << " mmol/L" << flagTG(tg) << "\n";
        if (ua > 0)
            oss << "- 血尿酸：" << std::fixed << std::setprecision(0) << ua
                << " µmol/L" << flagUA(ua, isMale) << "\n";
    } else {
        oss << "\n## 临床检验\n无数据\n";
    }

    // 计算评估
    oss << "\n## 计算评估\n";
    if (bmi > 0)
        oss << "- BMI：" << std::fixed << std::setprecision(1) << bmi
            << "（" << bmiCategory << "）\n";
    if (ascvd > 0)
        oss << "- China-PAR 10年 ASCVD 风险："
            << std::fixed << std::setprecision(1) << ascvd
            << "%（" << ascvdCategory << "）\n";

    // 趋势
    if (!trendSummary.empty()) {
        oss << "\n## 近期趋势\n" << trendSummary << "\n";
    }

    oss << "\n---\n";
    oss << "请基于以上数据生成本次" << periodLabel << "。";

    return oss.str();
}

// ============================================================
// 工厂函数
// ============================================================
std::unique_ptr<LLMService> createLLMService() {
    return std::make_unique<LLMServiceImpl>();
}

} // namespace health