# OmniHealth AI 健康顾问 — RAG 架构实现方案

> 基于 cpp-httplib + DeepSeek API 的上下文注入（Context Injection）方案

---

## 一、数据提取方案

### 1.1 提取原则

- **脱敏**：绝对不发送 `name`、`id` 等个人身份信息
- **时效性**：体征和血压取近 7 天均值，检验取最新值
- **完整性**：包含 China-PAR 所需的全部参数以便 AI 解释风险

### 1.2 具体提取字段（周报 / 月报 双版本）

周报提取近 **7 天**数据，月报提取近 **30 天**数据。除时间窗口外，字段结构完全一致。

```
┌─────────────────────────────────────────────────────┐
│              发送给 LLM 的数据（脱敏后）              │
├─────────────────────────────────────────────────────┤
│ 基本画像（来自 UserProfile，去除 name/id）            │
│   age: int            ← birthDate 计算整岁           │
│   gender: "男"/"女"                                   │
│   smoking: "从不/已戒/吸烟"                           │
│   diabetes: true/false                                │
│   region: "北方/南方"                                 │
│   urban: true/false                                   │
│   familyHistory: true/false                           │
├─────────────────────────────────────────────────────┤
│ 体征（时段均值，来自 vitals_records）                  │
│   → 周报取近 7 天，月报取近 30 天                       │
│   avgHeartRate: double    ← AVG(heart_rate)          │
│   avgSteps: double        ← AVG(steps) 日均          │
│   avgSleep: double        ← AVG(sleep_hours)         │
│   avgWeight: double       ← AVG(weight_kg)           │
│   latestWaist: double     ← 最新 waist_cm            │
├─────────────────────────────────────────────────────┤
│ 血压（时段均值，来自 blood_pressure_records）          │
│   → 周报取近 7 天，月报取近 30 天                       │
│   avgSystolic: double     ← AVG(systolic)            │
│   avgDiastolic: double    ← AVG(diastolic)           │
│   bpCount: int            ← COUNT(*)                 │
├─────────────────────────────────────────────────────┤
│ 临床检验（最新值，来自 lab_test_records）             │
│   fastingGlucose: double                              │
│   totalCholesterol: double                            │
│   hdl: double                                         │
│   ldl: double                                         │
│   triglycerides: double                               │
│   uricAcid: double                                    │
├─────────────────────────────────────────────────────┤
│ 计算指标                                             │
│   bmi: double + category                              │
│   ascvdRisk: double + category (五级分层)             │
├─────────────────────────────────────────────────────┤
│ 趋势概览（近 7 天）                                  │
│   各指标均值 + 趋势方向 ↑↓→                           │
└─────────────────────────────────────────────────────┘
```

---

## 二、Prompt 设计

### 2.1 System Prompt（系统人设 + 强制 JSON 输出）

```
你是一位顶级的哈佛医学院心血管与代谢疾病专家，拥有 20 年临床经验。
你正在通过 OmniHealth 数字孪生系统为用户生成个性化健康{{periodLabel}}。
（周报基于近 7 天数据，月报基于近 30 天数据）

【重要规则】
1. 你的所有建议必须严格基于下方提供的用户真实健康数据，不得臆测或编造。
2. 如果某项数据缺失（标注为"无数据"），请明确说明该项无法评估，不要猜测。
3. 使用专业但通俗易懂的中文，避免过于晦涩的医学术语。
4. 所有建议必须附有免责声明："本建议仅供参考，不构成医疗诊断。如有不适请及时就医。"

【强制输出格式】
你必须严格返回合法的 JSON 格式，不包含任何 markdown 代码块标记，不包含任何 JSON 之外的文字。
JSON 结构如下：

{
  "risk_analysis": {
    "overall": "整体风险评估（2-3句话）",
    "cardiovascular": "心血管专项分析",
    "metabolic": "代谢专项分析",
    "alert_items": ["需要警惕的指标1", "需要警惕的指标2"]
  },
  "action_plan": {
    "diet": ["饮食建议1", "饮食建议2", "饮食建议3"],
    "exercise": ["运动建议1", "运动建议2"],
    "lifestyle": ["生活习惯建议1", "生活习惯建议2"],
    "monitoring": ["建议监测的指标1", "建议监测的指标2"]
  },
  "conclusion": "一句话核心结论",
  "disclaimer": "⚠️ 免责声明：本报告由 AI 生成，仅供参考，不构成医疗诊断或治疗建议。如有健康疑虑，请及时咨询专业医生。"
}
```

### 2.2 User Prompt（脱敏数据注入 + 用户问题）

模板如下（`{{}}` 为占位符，由 C++ 代码替换）：

```
以下是一位用户授权你读取的本地健康数据库摘要（数据已脱敏）：

## 基本画像
- 年龄：{{age}} 岁
- 性别：{{gender}}
- 吸烟状况：{{smoking}}
- 糖尿病：{{diabetes}}
- 地域：{{region}}（{{urbanRural}}）
- 早发 ASCVD 家族史：{{familyHistory}}

## 近 {{days}} 天体徵均值（{{periodLabel}}）
- 心率：{{avgHeartRate}} bpm
- 日均步数：{{avgSteps}} 步
- 日均睡眠：{{avgSleep}} 小时
- 体重：{{avgWeight}} kg
- 腰围（最新）：{{waist}} cm

## 近 {{days}} 天血压均值（共 {{bpCount}} 次测量）
- 收缩压：{{avgSystolic}} mmHg
- 舒张压：{{avgDiastolic}} mmHg

## 最新临床检验
- 空腹血糖：{{glucose}} mmol/L {{glucoseFlag}}
- 总胆固醇：{{tc}} mmol/L {{tcFlag}}
- HDL-C：{{hdl}} mmol/L {{hdlFlag}}
- LDL-C：{{ldl}} mmol/L {{ldlFlag}}
- 甘油三酯：{{tg}} mmol/L {{tgFlag}}
- 血尿酸：{{ua}} µmol/L {{uaFlag}}

## 计算评估
- BMI：{{bmi}}（{{bmiCategory}}）
- China-PAR 10年 ASCVD 风险：{{ascvd}}%（{{ascvdCategory}}）

## 近期趋势
{{trendSummary}}

---

{{userQuery}}
```

### 2.3 异常标记逻辑（`{{xxxFlag}}`）

后端在拼接时自动判定：

| 指标 | 正常范围 | 偏高标记 | 偏低标记 |
|------|---------|---------|---------|
| 空腹血糖 | 3.9-6.1 mmol/L | `⚠️ 偏高` | — |
| 总胆固醇 | <5.2 mmol/L | `⚠️ 偏高` | — |
| HDL-C | >1.0(男)/1.3(女) | — | `⚠️ 偏低` |
| LDL-C | <3.4 mmol/L | `⚠️ 偏高` | — |
| 甘油三酯 | <1.7 mmol/L | `⚠️ 偏高` | — |
| 血尿酸 | <420(男)/360(女) | `⚠️ 偏高` | — |
| BMI | 18.5-24 | `⚠️ 超重/肥胖` | `⚠️ 偏瘦` |
| 收缩压 | <140 mmHg | `⚠️ 偏高` | — |
| 舒张压 | <90 mmHg | `⚠️ 偏高` | — |

---

## 三、架构设计

### 3.1 文件变更清单

| 文件 | 操作 | 内容 |
|------|------|------|
| `backend/CMakeLists.txt` | 修改 | 添加 cpp-httplib (FetchContent) |
| `backend/include/LLMService.h` | 修改 | 简化接口，新增结构化方法 |
| `backend/src/LLMService.cpp` | **新建** | HTTP 通信 + Prompt 组装 + JSON 解析 |
| `backend/include/HealthManager.h` | 修改 | 新增 `askAIAdvisor(query)` 返回 JSON |
| `backend/src/HealthManager.cpp` | 修改 | 实现 askAIAdvisor，完成数据提取→LLM→解析链 |
| `frontend/CMakeLists.txt` | 无需改动 | — |

### 3.2 类设计

```cpp
// LLMService.h — 精简后的接口
class LLMService {
public:
    /// @brief 配置 API 连接（支持环境变量 OPENAI_API_KEY）
    bool configure(const std::string& endpoint,
                   const std::string& apiKey = "",
                   const std::string& model = "deepseek-chat");

    /// @brief 发送对话请求，返回 JSON 字符串
    std::string chat(const std::string& systemPrompt,
                     const std::string& userMessage);

    /// @brief 组装脱敏后的健康上下文 Prompt
    /// @param profile 用户档案（会自动去除 name/id）
    /// @param vitals  时段内体征记录（7天或30天）
    /// @param bps     时段内血压记录（7天或30天）
    /// @param labs    全部检验记录（取最新）
    /// @param bmi     BMI 值 + 分级
    /// @param ascvd   ASCVD 风险值 + 分级
    /// @param trends  趋势摘要文本
    /// @param days    数据天数（7 或 30），用于填充模板 `{{days}}`
    /// @param periodLabel 周期标签（"周报"或"月报"），用于填充 `{{periodLabel}}`
    /// @return 格式化的 User Prompt 字符串
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
};
```

### 3.3 数据流

```
┌──────────────┐    ┌──────────────────┐    ┌─────────────────┐
│  HealthMgr   │───▶│   LLMService     │───▶│  DeepSeek API   │
│  (数据提取)   │    │  (Prompt组装+HTTP)│    │  (云端推理)      │
└──────────────┘    └──────────────────┘    └────────┬────────┘
       │                                              │
       │ 1. getUserProfile()                         │ JSON 回复
       │ 2. getVitalsRecords(7天)                    │
       │ 3. getBloodPressureRecords(7天)  ┌──────────┘
       │ 4. getLabTestRecords(最新)        │
       │ 5. calculateBMI() + ASCVD        ▼
       │ 6. analyzeTrendReport(7天)  ┌──────────────┐
       │                             │  nlohmann    │
       │                             │  json::parse │
       │                             └──────┬───────┘
       │                                    │
       ▼                                    ▼
  JSON 字符串返回给前端              risk_analysis
  (前端 Qt 解析渲染)                action_plan
                                    conclusion
                                    disclaimer
```

### 3.4 错误处理策略

| 场景 | 行为 |
|------|------|
| API Key 未配置 | 返回 `{"error": "请先配置 API Key"}` |
| 网络超时（>30s） | 返回 `{"error": "网络超时，无法连接 AI 顾问"}` |
| HTTP 4xx/5xx | 返回 `{"error": "AI 服务异常 (HTTP xxx)"}` |
| JSON 解析失败 | 返回 `{"error": "AI 返回格式异常，请重试"}` + 兜底文本 |
| 数据库无数据 | User Prompt 中标注"无数据"，AI 自行处理 |

---

## 四、CMakeLists.txt 集成 cpp-httplib

```cmake
# cpp-httplib (Header-only HTTP 库，用于 LLM API 调用)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.3
)
FetchContent_MakeAvailable(httplib)

# 链接时需添加 SSL 支持
target_link_libraries(health_backend PUBLIC
    SQLite3::SQLite3
    nlohmann_json::nlohmann_json
    httplib::httplib
)

# macOS: 链接系统 SSL
if(APPLE)
    target_link_libraries(health_backend PUBLIC
        "-framework Security"
        "-framework CoreFoundation"
    )
endif()
```

> Windows 下 cpp-httplib 使用 WinHTTP/WinSSL，无需额外链接。

---

## 五、新增 HealthManager 接口 + AI-First 策略

### 5.1 统一入口：`generateAIReport(ReportPeriod)`

```cpp
// HealthManager.h

/// @brief 生成 AI 驱动的个性化健康报告（RAG 上下文注入）
/// @param period WEEKLY（近7天）或 MONTHLY（近30天）
/// @return JSON 格式的 AI 回复（含 risk_analysis / action_plan / conclusion / disclaimer）
///
/// 【AI-First 策略】：
///   1. 优先尝试调用 LLM API 生成 AI 报告
///   2. 如果 LLMService 未配置（无 API Key / 无网络）→ 自动降级为本地报告
///   3. 本地报告即调用 generateHealthReport(period)（基于时段均值的纯文本）
///
/// 该接口自动完成：
///   1. 根据 period 提取近 7/30 天体徵/血压均值 + 最新检验 + BMI + ASCVD + 趋势
///   2. 脱敏组装 System Prompt + User Prompt
///   3. 调用 LLMService::chat() 发送请求
///   4. 解析返回 JSON，确保格式正确
///   5. 失败时自动回退到本地 generateHealthReport(period)
virtual std::string generateAIReport(ReportPeriod period) = 0;
```

### 5.2 AI-First / 离线兜底 决策树

```
generateAIReport(period)
        │
        ▼
  LLMService 已配置？
   (apiKey 非空且 endpoint 可达)
        │
   ┌────┴────┐
   │ YES     │ NO
   ▼         ▼
 调用 AI   generateHealthReport(period)
   │        （本地纯文本报告）
   │
   ▼
 HTTP 请求成功？
   │
 ┌─┴─┐
 │YES│NO
 ▼   ▼
解析  降级本地报告
JSON  +
返回  "⚠️ AI 服务暂不可用\n\n"
      + generateHealthReport(period)
```

### 5.3 与前端队友的契约

前端只需关心两个接口：

| 场景 | 调用 | 返回格式 |
|------|------|---------|
| 想用 AI 报告 | `generateAIReport(WEEKLY/MONTHLY)` | JSON（AI 成功）或纯文本（降级兜底） |
| 纯本地离线报告 | `generateHealthReport(WEEKLY/MONTHLY)` | 纯文本（必定成功，无网络依赖） |

前端可以先用 `generateAIReport()` 尝试获取 AI 报告，如果返回的 JSON 中包含 `"error"` 字段或不是合法 JSON，则展示降级文本。

---

## 六、实施步骤

### Phase 1: 基础设施（我来做）
1. `backend/CMakeLists.txt` — 添加 cpp-httplib FetchContent + SSL 链接
2. `backend/src/LLMService.cpp` — **新建**，实现 HTTP 通信 + Prompt 组装 + JSON 解析
3. `backend/include/LLMService.h` — 精简接口，新增 `buildHealthContextPrompt` 静态方法

### Phase 2: 业务集成（我来做）
4. `backend/include/HealthManager.h` — 新增 `generateAIReport(ReportPeriod)` 接口 + AI-First 决策逻辑
5. `backend/src/HealthManager.cpp` — 实现数据提取 → Prompt 组装 → LLM 调用 → 降级兜底全链路
6. `backend/src/HealthManager.cpp` — `buildHealthContextPrompt` 支持 `{{days}}` / `{{periodLabel}}` 模板变量

### Phase 3: 测试（我来做）
7. 写一个 CLI 测试：分别测试 AI 周报/月报 + 离线降级 + 本地周报/月报

### Phase 4: 前端对接（队友做）
8. 用 `generateAIReport(WEEKLY/MONTHLY)` → 尝试解析 JSON → Qt 卡片渲染
9. JSON 解析失败或含 `error` → 展示纯文本降级报告
