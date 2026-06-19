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
    std::string model_ = "deepseek-v4-pro";
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
    model_    = model.empty() ? "deepseek-v4-pro" : model;

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
    cli.set_follow_location(true);    // 跟随 HTTP 重定向
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
    requestBody["max_tokens"] = 4096;

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
1. 你的所有分析必须严格基于下方提供的用户真实健康数据，不得臆测或编造。
2. 如果某项数据缺失（标注为"无数据"），请明确说明该项无法评估，不要猜测。
3. 使用专业但通俗易懂的中文，每项分析不少于 100 字，给出有深度、有价值的个性化解读。
4. 对比每个异常指标与正常参考范围的偏离程度，解释其临床意义。
5. 对于趋势标记 ↑ 或 ↓，分析变化方向是否值得担忧。
6. 每条建议必须具体、可执行，杜绝泛泛而谈（如"多吃蔬菜"不可接受，应写"每日摄入深色蔬菜≥300g"）。
7. 报告的结尾，请添加一句【追问引导】，鼓励用户针对报告中不理解或想深入了解的内容继续提问。

【强制输出格式】
你必须严格返回合法的 JSON 格式，不包含任何 markdown 代码块标记，不包含任何 JSON 之外的文字。
JSON 结构如下：

{
  "risk_analysis": {
    "overall": "整体风险评估（至少150字。串联 ASCVD 风险分层、异常指标数量、近期趋势方向，给出综合风险画像）",
    "cardiovascular": "心血管专项分析（至少100字。血压分级解读、血脂四项逐一分析、ASCVD 风险构成因素拆解）",
    "metabolic": "代谢专项分析（至少100字。血糖、BMI、腰围、尿酸的联合解读，评估代谢综合征可能性）",
    "alert_items": ["逐个列出所有值得警惕的异常指标，附带偏离程度说明"]
  },
  "action_plan": {
    "diet": ["至少4条具体饮食建议，每条包含量化目标（克数/份数/频率）"],
    "exercise": ["至少2条运动建议，包含类型、强度、时长、频率"],
    "lifestyle": ["至少3条生活习惯建议，包含具体行为改变"],
    "monitoring": ["至少3条监测建议，包含测量频率和警戒阈值"]
  },
  "conclusion": "一句话核心结论 + 最关键的 1-2 个立即行动项",
  "follow_up_prompt": "💬 您可能还想了解：【列出3-4个与当前数据高度相关的追问方向，例如'我应该怎么控制血糖？'、'如何有效降低收缩压？'、'我的饮食结构应该做哪些调整？'等】",
  "disclaimer": "⚠️ 免责声明：本报告由 AI 生成，仅供参考，不构成医疗诊断或治疗建议。如有健康疑虑，请及时咨询专业医生。"
})";
}

// ============================================================
// buildFollowUpSystemPrompt — 追问系统人设
// ============================================================
std::string LLMService::buildFollowUpSystemPrompt(const std::string& healthContext) {
    return R"(你是一位顶级的哈佛医学院心血管与代谢疾病专家，拥有 20 年临床经验。
你正在通过 OmniHealth 数字孪生系统回答用户对其健康报告的追问。

【用户的健康数据上下文（务必仔细阅读）】
)" + healthContext + R"(

【重要规则】
1. 回答必须严格基于上述健康数据，不得臆测或编造。
2. 如果用户的问题涉及数据中未包含的信息（如具体的药物使用、手术史、过敏史），
   请明确说明"您尚未录入该信息，建议先补充后再获取更精准的建议"。
3. 给出具体、量化、可操作的建议。杜绝泛泛而谈。
4. 使用专业但通俗易懂的中文。

【个性化安全约束（极其重要！必须遵守！）】
5. 年龄适配：务必根据用户的年龄推荐合适的运动和饮食。
   - 60岁以上用户：禁止推荐高强度/高冲击运动（如跑步、跳跃、大重量举重）。
   - 75岁以上用户：以平衡训练和低强度活动为主。
6. 病史禁忌：务必根据用户的既往病史排除禁忌建议。
   - 有高血压（收缩压≥140）的用户：禁止推荐需要憋气的运动（如举重、仰卧起坐），
     提醒运动前测量血压，收缩压≥160时暂停运动。
   - 有糖尿病的用户：提醒防低血糖（随身携带糖果），注意足部保护（穿专业运动鞋），
     避免空腹运动。
   - 有高尿酸/痛风的用户：禁止推荐高嘌呤食物（动物内脏、浓汤），避免剧烈无氧运动
     （会堆积乳酸抑制尿酸排泄），强调每日饮水≥2000ml。
   - 有吸烟史的用户：任何建议中都必须强调戒烟是降低风险的最优先事项。
   - BMI≥28 或腰围超标（男≥90/女≥85）的用户：优先推荐低冲击运动（游泳、快走、
     固定单车）保护关节。
   - 如果用户数据中有过敏标记或特定食物不耐受，务必避开相关食物建议。
7. 药物交互提醒：如果用户正在服药（特别是降压药、降糖药、抗凝药），在给出建议时
   提醒用户"请咨询医生后再调整，本建议不能替代药物处方"。

【自由文本模式】
追问环节不需要返回 JSON 格式，请用自然段落回复用户的问题。
如果适合以结构化方式呈现（如饮食计划、运动方案），可以使用清晰的标题分段。

【回答末尾必须包含】
- 免责声明："我是基于您提供的健康数据生成的数字健康助手。以上分析不构成医疗处方，
  不能替代执业医师的面对面诊断。"
)";
}

// ============================================================
// buildFollowUpUserPrompt — 追问用户消息
// ============================================================
std::string LLMService::buildFollowUpUserPrompt(const std::string& userQuestion) {
    return "用户针对上述健康数据提出了以下追问：\n\n" + userQuestion +
           "\n\n请基于用户的真实健康数据，给出专业、具体、个性化的回答。";
}

// ============================================================
// buildHealthContextPrompt — 脱敏数据组装 + 异常标记
// ============================================================
std::string LLMService::buildHealthContextPrompt(
    const UserProfile& profile,
    const std::vector<VitalsRecord>& vitals,
    const std::vector<BloodPressureRecord>& bps,
    const std::vector<LabTestRecord>& labs,
    const std::vector<MedicalHistoryRecord>& medicalHistory,
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

    // 病历摘要（脱敏，只传分类和内容，不传时间戳和ID）
    if (!medicalHistory.empty()) {
        oss << "\n## 用户既往病历摘要\n";
        oss << "以下信息来自用户手动录入，请在给出任何饮食、运动、用药建议前务必参考：\n\n";
        for (const auto& mh : medicalHistory) {
            oss << "【" << mh.category << "】" << mh.content << "\n";
        }
    }

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