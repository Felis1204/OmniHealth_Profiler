# HealthManager 前端 API 参考手册

> **版本**: 基于 `HealthManager.h` (backend/include/)
> **语言**: C++17/20
> **适用对象**: 前端开发人员（Qt / 其他 UI 框架）
> **核心原则**: 前端只能通过本手册中列出的 `HealthManager` 公开接口与后端交互，严禁直接操作数据库、网络或任何第三方后端库。

---

## 目录

1. [快速开始](#1-快速开始)
2. [工厂函数](#2-工厂函数)
3. [数据结构参考](#3-数据结构参考)
4. [CRUD 操作](#4-crud-操作)
5. [风险评估](#5-风险评估)
6. [趋势分析](#6-趋势分析)
7. [健康报告](#7-健康报告)
8. [AI 功能](#8-ai-功能)
9. [统计摘要](#9-统计摘要)
10. [时间处理](#10-时间处理)
11. [Qt 集成示例](#11-qt-集成示例)
12. [错误处理最佳实践](#12-错误处理最佳实践)
13. [完整接口速查表](#13-完整接口速查表)

---

## 1. 快速开始

### 1.1 唯一需要包含的头文件

```cpp
#include "HealthManager.h"
```

> 这是前端所需的**唯一**后端头文件。所有数据结构和接口都通过它引入。

### 1.2 创建 HealthManager 实例

```cpp
#include <memory>
#include "HealthManager.h"

// 通过工厂函数创建实例（返回 unique_ptr，自动管理生命周期）
auto mgr = health::createHealthManager();
```

`createHealthManager()` 返回 `std::unique_ptr<HealthManager>`。HealthManager 是一个抽象基类，实际实现隐藏在 `.cpp` 文件中（PIMPL 模式），前端无需关心内部细节。

### 1.3 命名空间

所有后端类型均位于 `health` 命名空间下。建议在 `.cpp` 文件中使用 `using namespace health;`，但在头文件中使用完整限定名。

### 1.4 最简单的数据写入示例

```cpp
#include "HealthManager.h"
#include <chrono>

int main() {
    auto mgr = health::createHealthManager();

    // 创建并上传一条血压记录
    health::BloodPressureRecord bp;
    bp.id = "bp-001";
    bp.timestamp = std::chrono::system_clock::now();
    bp.recordType = health::HealthRecordType::BP;
    bp.systolic = 128;
    bp.diastolic = 82;

    if (mgr->addBloodPressureRecord(bp)) {
        // 写入成功
    }
}
```

---

## 2. 工厂函数

```cpp
namespace health {
    std::unique_ptr<HealthManager> createHealthManager();
}
```

| 项目 | 说明 |
|------|------|
| 返回值 | `std::unique_ptr<HealthManager>` |
| 生命周期 | 由 `unique_ptr` 自动管理，析构时释放后端资源 |
| 线程安全 | 当前版本不支持多线程并发调用，所有操作应在同一线程执行 |
| 单例 vs 多实例 | 可以创建多个实例，但数据层共享同一个 SQLite 数据库文件 |

**推荐用法**：在应用程序启动时创建一次，作为全局或 App 级成员持有。

```cpp
// 在 QApplication 派生类或主窗口类中
class HealthApp {
private:
    std::unique_ptr<health::HealthManager> healthMgr_;
public:
    HealthApp() : healthMgr_(health::createHealthManager()) {}
    health::HealthManager* manager() { return healthMgr_.get(); }
};
```

---

## 3. 数据结构参考

### 3.1 基类：HealthRecord

所有健康记录都派生自 `HealthRecord`，包含公共字段：

```cpp
struct HealthRecord {
    std::string id;                          // UUID 主键，建议用 QUuid 生成
    HealthRecordType recordType;             // 记录类型枚举（由子类赋值）
    TimePoint timestamp;                     // 采集/录入时间（system_clock::time_point）
    std::optional<std::string> source;       // 数据来源（手动 / 设备型号）
    std::optional<std::string> note;         // 自由备注
};
```

### 3.2 HealthRecordType 枚举

```cpp
enum class HealthRecordType {
    VITALS,     // 基础体征
    LAB_TEST,   // 临床检验
    BP,         // 血压
    HISTORY     // 病历摘要
};
```

### 3.3 VitalsRecord（体征记录）

```cpp
struct VitalsRecord : public HealthRecord {
    std::optional<double> heartRate;   // 心率 (bpm)
    std::optional<int>    steps;       // 步数
    std::optional<double> sleepHours;  // 睡眠时长 (小时)
    std::optional<double> weightKg;    // 体重 (kg)
    std::optional<double> heightCm;    // 身高 (cm)，用于 BMI 计算
    std::optional<double> waistCm;     // 腰围 (cm)，用于代谢综合征 / ASCVD 评估
};
```

**创建示例**：

```cpp
health::VitalsRecord v;
v.id = "v-001";
v.timestamp = std::chrono::system_clock::now();
v.recordType = health::HealthRecordType::VITALS;
v.heartRate = 72;
v.steps = 8500;
v.sleepHours = 7.5;
v.weightKg = 68.0;
v.heightCm = 172.0;
v.waistCm = 80.0;
v.source = "手动录入";
```

### 3.4 LabTestRecord（临床检验记录）

```cpp
struct LabTestRecord : public HealthRecord {
    std::optional<double> fastingGlucose;     // 空腹血糖 (mmol/L)
    std::optional<double> totalCholesterol;   // 总胆固醇 (mmol/L)
    std::optional<double> ldlC;               // 低密度脂蛋白 (mmol/L)
    std::optional<double> hdlC;               // 高密度脂蛋白 (mmol/L)
    std::optional<double> triglycerides;      // 甘油三酯 (mmol/L)
    std::optional<double> uricAcid;           // 血尿酸 (μmol/L)
};
```

**创建示例**：

```cpp
health::LabTestRecord lab;
lab.id = "lab-001";
lab.timestamp = std::chrono::system_clock::now();
lab.recordType = health::HealthRecordType::LAB_TEST;
lab.fastingGlucose = 5.2;
lab.totalCholesterol = 4.8;
lab.hdlC = 1.3;
lab.ldlC = 2.9;
lab.triglycerides = 1.5;
lab.uricAcid = 360.0;
lab.source = "XX医院检验科";
```

### 3.5 BloodPressureRecord（血压记录）

```cpp
struct BloodPressureRecord : public HealthRecord {
    std::optional<int> systolic;   // 收缩压 (mmHg)
    std::optional<int> diastolic;  // 舒张压 (mmHg)
};
```

**创建示例**：

```cpp
health::BloodPressureRecord bp;
bp.id = "bp-001";
bp.timestamp = std::chrono::system_clock::now();
bp.recordType = health::HealthRecordType::BP;
bp.systolic = 128;
bp.diastolic = 82;
bp.source = "欧姆龙电子血压计";
```

### 3.6 MedicalHistoryRecord（病历摘要记录）

```cpp
struct MedicalHistoryRecord : public HealthRecord {
    std::string category;  // 分类标签
    std::string content;   // 自由文本内容
};
```

**category 的合法值**（字符串，非枚举）：

| 取值 | 含义 |
|------|------|
| `"既往病史"` | 既往病史 |
| `"手术史"` | 手术史 |
| `"过敏史"` | 过敏史 |
| `"家族史"` | 家族史 |
| `"用药史"` | 用药史 |
| `"其他"` | 其他 |

**创建示例**：

```cpp
health::MedicalHistoryRecord mh;
mh.id = "mh-001";
mh.timestamp = std::chrono::system_clock::now();
mh.recordType = health::HealthRecordType::HISTORY;
mh.category = "过敏史";
mh.content = "青霉素过敏，曾出现皮疹";
```

### 3.7 UserProfile（用户档案）

```cpp
struct UserProfile {
    std::string id;                               // UUID 主键
    std::string name;                             // 姓名（必填）
    std::optional<std::string> birthDate;         // 出生日期，格式 "YYYY-MM-DD"
    std::optional<std::string> gender;            // "MALE" / "FEMALE"
    std::optional<std::string> smokingStatus;     // "NEVER" / "FORMER" / "CURRENT"
    std::optional<std::string> region;            // "NORTH" / "SOUTH"（China-PAR 地域）
    std::optional<std::string> urbanRural;        // "URBAN" / "RURAL"（China-PAR 城乡）
    std::optional<bool> familyHistoryASCVD;       // 早发 ASCVD 家族史
    std::optional<bool> hasDiabetes;              // 确诊糖尿病
};
```

**创建示例**：

```cpp
health::UserProfile profile;
profile.id = "user-001";
profile.name = "张三";
profile.birthDate = "1985-06-15";
profile.gender = "MALE";
profile.smokingStatus = "FORMER";
profile.region = "NORTH";
profile.urbanRural = "URBAN";
profile.familyHistoryASCVD = false;
profile.hasDiabetes = false;
mgr->saveUserProfile(profile);  // upsert：存在则更新，不存在则插入
```

---

## 4. CRUD 操作

所有 CRUD 方法遵循统一的命名和签名模式：

- **addXxxRecord** — 新增记录
- **updateXxxRecord** — 更新记录（按 id 匹配）
- **deleteXxxRecord** — 删除记录（按 id 匹配）
- **getXxxRecords** — 按时间范围查询记录

### 4.1 体征记录 (VitalsRecord)

```cpp
bool addVitalsRecord(const VitalsRecord& record);
bool updateVitalsRecord(const VitalsRecord& record);
bool deleteVitalsRecord(const std::string& id);
std::vector<VitalsRecord> getVitalsRecords(
    std::optional<TimePoint> from,
    std::optional<TimePoint> to
) const;
```

**示例**：

```cpp
// 查询所有体征记录
auto all = mgr->getVitalsRecords(std::nullopt, std::nullopt);

// 查询最近 7 天的体征记录
auto now = std::chrono::system_clock::now();
auto weekAgo = now - std::chrono::hours(24 * 7);
auto recent = mgr->getVitalsRecords(weekAgo, now);

// 删除一条记录
if (mgr->deleteVitalsRecord("v-001")) {
    // 删除成功
}
```

### 4.2 临床检验记录 (LabTestRecord)

```cpp
bool addLabTestRecord(const LabTestRecord& record);
bool updateLabTestRecord(const LabTestRecord& record);
bool deleteLabTestRecord(const std::string& id);
std::vector<LabTestRecord> getLabTestRecords(
    std::optional<TimePoint> from,
    std::optional<TimePoint> to
) const;
```

### 4.3 血压记录 (BloodPressureRecord)

```cpp
bool addBloodPressureRecord(const BloodPressureRecord& record);
bool updateBloodPressureRecord(const BloodPressureRecord& record);
bool deleteBloodPressureRecord(const std::string& id);
std::vector<BloodPressureRecord> getBloodPressureRecords(
    std::optional<TimePoint> from,
    std::optional<TimePoint> to
) const;
```

### 4.4 病历摘要记录 (MedicalHistoryRecord)

```cpp
bool addMedicalHistoryRecord(const MedicalHistoryRecord& record);
bool updateMedicalHistoryRecord(const MedicalHistoryRecord& record);
bool deleteMedicalHistoryRecord(const std::string& id);
// 注意：无时间范围参数，返回全部（按时间降序）
std::vector<MedicalHistoryRecord> getMedicalHistoryRecords() const;
```

**说明**：病历摘要记录不支持时间范围过滤，始终返回全部记录并按时间降序排列（最近的在前面）。

### 4.5 用户档案 (UserProfile)

```cpp
bool saveUserProfile(const UserProfile& profile);    // upsert 模式
std::optional<UserProfile> getUserProfile() const;   // 返回 nullopt 表示无数据
bool deleteUserProfile(const std::string& id);
```

**upsert 说明**：`saveUserProfile` 是 upsert 操作——如果数据库中已存在记录（按 id 匹配）则更新，否则插入新记录。

**示例**：

```cpp
// 获取当前用户档案
auto profileOpt = mgr->getUserProfile();
if (profileOpt.has_value()) {
    auto& profile = profileOpt.value();
    // 修改吸烟状态
    profile.smokingStatus = "CURRENT";
    mgr->saveUserProfile(profile);  // 更新已有档案
}
```

### 4.6 CRUD 通用注意事项

- **返回值**：所有 add/update/delete 方法返回 `bool`，true 表示成功。
- **id 生成**：前端负责生成 UUID。Qt 中使用 `QUuid::createUuid().toString().toStdString()`。
- **timestamp 设置**：前端负责设置时间戳，建议使用 `std::chrono::system_clock::now()`。
- **recordType 设置**：前端在创建记录时必须正确设置 `recordType` 字段。

---

## 5. 风险评估

### 5.1 ASCVD 评估（10年动脉粥样硬化性心血管疾病风险）

```cpp
double calculateASCVDScore() const;
```

- **返回**：10 年 ASCVD 风险百分比（`0.0 — 100.0`），基于 China-PAR 模型。
- **数据来源**：自动从数据库提取用户档案（年龄、性别、吸烟、地域、家族史、糖尿病史）、最近血压数据、最近检验数据和最近体征数据（腰围）。
- **风险分层**：使用 `ASCVDCalculator::getRiskCategory()` 将百分比转为中文描述。

```cpp
#include "ASCVDCalculator.h"

double score = mgr->calculateASCVDScore();
std::string category = health::ASCVDCalculator::getRiskCategory(score);
// 输出示例：score = 3.2, category = "低危"
```

**风险等级对照表**：

| 风险百分比 | 等级 |
|-----------|------|
| < 2.5% | 低危 |
| 2.5% — 5.0% | 临界风险 |
| 5.0% — 10.0% | 中危 |
| 10.0% — 20.0% | 高危 |
| >= 20.0% | 极高危 |

### 5.2 BMI 计算

```cpp
double calculateBMI() const;
std::string getBMICategory() const;
```

- `calculateBMI()` 返回 `体重(kg) / 身高(m)²`。
- `getBMICategory()` 返回中文分级描述。

**BMI 分级对照表**：

| BMI 范围 | 等级 |
|---------|------|
| < 18.5 | 偏瘦 |
| 18.5 — 23.9 | 正常 |
| 24.0 — 27.9 | 超重 |
| >= 28.0 | 肥胖 |

**示例**：

```cpp
double bmi = mgr->calculateBMI();
std::string category = mgr->getBMICategory();
// 输出示例：bmi = 23.1, category = "正常"
```

### 5.3 TyG 指数（胰岛素抵抗筛查）

```cpp
MetabolicResult calculateTyGIndex() const;
```

- **公式**：`TyG = ln( TG(mg/dL) × FPG(mg/dL) / 2 )`
- **返回**：`MetabolicResult { double score; std::string riskLevel; }`
- **风险分层**：
  - `TyG < 8.70` → `"Low Risk (胰岛素抵抗风险低)"`
  - `TyG ≥ 8.70` → `"High Risk (存在胰岛素抵抗，代谢综合征高危)"`

**示例**：

```cpp
auto result = mgr->calculateTyGIndex();
// result.score = 9.12
// result.riskLevel = "High Risk (存在胰岛素抵抗，代谢综合征高危)"
```

### 5.4 CDRS（中国糖尿病风险评分）

```cpp
MetabolicResult calculateCDRS() const;
```

- **公式**：累加模型 `TotalScore = AgeScore + WaistScore + FamilyScore`
- **返回**：`MetabolicResult { double score; std::string riskLevel; }`
- **风险分层**（按性别独立切点）：
  - 男性 `≥ 17` 或 女性 `≥ 14` → `"High Risk (高度疑似隐匿性糖尿病，建议OGTT)"`
  - 否则 → `"Low Risk (糖尿病风险较低)"`

**示例**：

```cpp
auto result = mgr->calculateCDRS();
// result.score = 20.0
// result.riskLevel = "High Risk (高度疑似隐匿性糖尿病，建议OGTT)"
```

### 5.5 MetabolicResult 结构体

```cpp
struct MetabolicResult {
    double score;            // 连续评分值
    std::string riskLevel;   // 风险定性描述（中英文混合，可直接展示）
};
```

---

## 6. 趋势分析

### 6.1 旧版 analyzeTrend（向后兼容）

```cpp
TrendResult analyzeTrend(HealthRecordType type, TimePoint from, TimePoint to) const;
```

- 返回默认指标的统计值（如 VITALS 返回心率，BP 返回收缩压）。
- **不推荐新代码使用**，请迁移到 `analyzeTrendReport`。

```cpp
struct TrendResult {
    double average;  // 均值
    double min;      // 最小值
    double max;      // 最大值
    double median;   // 中位数
    double slope;    // 线性回归斜率（正=上升趋势，负=下降趋势）
};
```

### 6.2 新版 analyzeTrendReport（推荐）

```cpp
TrendReport analyzeTrendReport(
    HealthRecordType type,
    std::optional<TimePoint> from,
    std::optional<TimePoint> to
) const;
```

这是**推荐的趋势分析接口**，返回完整的时间序列数据和统计摘要，专为图表绘制设计。

#### 6.2.1 TrendReport 结构体

```cpp
struct TrendReport {
    HealthRecordType recordType;          // VITALS / LAB_TEST / BP（HISTORY 不参与趋势）
    std::string title;                    // 如 "体征指标趋势分析"
    std::string periodLabel;              // 如 "2024-06-01 至 2024-06-18"
    std::vector<MetricTrend> metrics;     // 每个可量化指标一条折线
    bool isEmpty() const;                 // 无有效数据时返回 true
};
```

#### 6.2.2 MetricTrend 结构体

```cpp
struct MetricTrend {
    std::string metricName;               // 指标中文名，如 "收缩压"
    std::string metricKey;                // 指标键，如 "systolic"（用于程序匹配/排序）
    std::string unit;                     // 单位，如 "mmHg"、"kg"、"mmol/L"
    std::vector<TrendPoint> dataPoints;   // 时间-数值数据点（按时间升序）

    double average = 0.0;    // 均值
    double min     = 0.0;    // 最小值
    double max     = 0.0;    // 最大值
    double median  = 0.0;    // 中位数
    double slope   = 0.0;    // 线性回归斜率
    int    count   = 0;      // 数据点个数
};
```

#### 6.2.3 TrendPoint 结构体

```cpp
struct TrendPoint {
    std::string timestamp;   // ISO 8601 字符串，如 "2024-06-15T08:00:00Z"
    double value;            // 指标数值
};
```

#### 6.2.4 各记录类型对应的指标

| 记录类型 | metricKey | metricName | unit |
|---------|-----------|------------|------|
| VITALS | heartRate | 心率 | bpm |
| VITALS | steps | 步数 | 步 |
| VITALS | sleepHours | 睡眠时长 | 小时 |
| VITALS | weightKg | 体重 | kg |
| VITALS | heightCm | 身高 | cm |
| VITALS | waistCm | 腰围 | cm |
| LAB_TEST | fastingGlucose | 空腹血糖 | mmol/L |
| LAB_TEST | totalCholesterol | 总胆固醇 | mmol/L |
| LAB_TEST | ldlC | 低密度脂蛋白 | mmol/L |
| LAB_TEST | hdlC | 高密度脂蛋白 | mmol/L |
| LAB_TEST | triglycerides | 甘油三酯 | mmol/L |
| LAB_TEST | uricAcid | 血尿酸 | μmol/L |
| BP | systolic | 收缩压 | mmHg |
| BP | diastolic | 舒张压 | mmHg |

#### 6.2.5 使用示例

```cpp
#include "HealthManager.h"
#include <chrono>
#include <iostream>

void analyzeBloodPressure(health::HealthManager* mgr) {
    auto now = std::chrono::system_clock::now();
    auto monthAgo = now - std::chrono::hours(24 * 30);

    auto report = mgr->analyzeTrendReport(
        health::HealthRecordType::BP, monthAgo, now
    );

    if (report.isEmpty()) {
        std::cout << "该时间范围内无血压数据" << std::endl;
        return;
    }

    std::cout << "报告标题：" << report.title << std::endl;
    std::cout << "时间范围：" << report.periodLabel << std::endl;

    for (const auto& metric : report.metrics) {
        std::cout << "\n指标：" << metric.metricName
                  << " (" << metric.unit << ")" << std::endl;
        std::cout << "  数据点数：" << metric.count << std::endl;
        std::cout << "  均值：" << metric.average << std::endl;
        std::cout << "  范围：" << metric.min << " — " << metric.max << std::endl;
        std::cout << "  趋势斜率：" << metric.slope
                  << " (" << (metric.slope > 0 ? "上升" : "下降") << ")" << std::endl;

        // 打印每个数据点
        for (const auto& pt : metric.dataPoints) {
            std::cout << "    " << pt.timestamp << " → " << pt.value << std::endl;
        }
    }
}
```

---

## 7. 健康报告

### 7.1 generateHealthReport() — 健康快照（无参数）

```cpp
std::string generateHealthReport() const;
```

- **返回**：基于最新单点数据的健康报告文本（纯文本）。
- **用途**：向后兼容旧版前端；显示"当前健康状况一览"。

### 7.2 generateHealthReport(ReportPeriod) — 周期性报告

```cpp
std::string generateHealthReport(ReportPeriod period) const;
```

- **参数**：
  - `ReportPeriod::WEEKLY` — 近 7 天均值
  - `ReportPeriod::MONTHLY` — 近 30 天均值
- **返回**：包含各指标均值 + BMI + ASCVD + 趋势概览的格式化报告文本。

### 7.3 报告周期枚举

```cpp
enum class ReportPeriod {
    WEEKLY,   // 近 7 天
    MONTHLY   // 近 30 天
};
```

### 7.4 使用示例

```cpp
// 健康快照
std::string snapshot = mgr->generateHealthReport();
// 在 UI 中显示 snapshot

// 周报
std::string weekly = mgr->generateHealthReport(
    health::HealthManager::ReportPeriod::WEEKLY
);

// 月报
std::string monthly = mgr->generateHealthReport(
    health::HealthManager::ReportPeriod::MONTHLY
);
```

---

## 8. AI 功能

> AI 功能依赖 LLM（大语言模型）服务。系统当前使用 DeepSeek API（兼容 OpenAI 接口格式）。

### 8.1 前置条件：配置 API 连接

**新方式（推荐）— 通过 UI 或代码配置**：

```cpp
// 方式 1：在代码中直接配置
mgr->configureLLM(
    "https://api.deepseek.com/chat/completions",  // endpoint
    "sk-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",         // apiKey
    "deepseek-v4-pro"                              // model
);

// 方式 2：让用户通过 AI 设置对话框配置（推荐）
AISettingsDialog dlg(mgr.get(), this);
dlg.exec();
```

**旧方式（环境变量，仍支持）**：

```bash
export OPENAI_API_KEY="sk-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
```

如果 apiKey 参数为空字符串，`configureLLM()` 会自动从环境变量 `OPENAI_API_KEY` 读取。

### 8.2 检查 AI 配置状态

```cpp
// 检查 LLM 是否已配置
if (mgr->isLLMConfigured()) {
    // AI 功能可用
    auto report = mgr->generateAIReport(period);
} else {
    // 提示用户配置 API
    QMessageBox::information(this, "提示",
        "请先在 AI 设置中配置 API Key。");
}
```

### 8.3 configureLLM() — 配置 LLM API 连接

```cpp
bool configureLLM(const std::string& endpoint,
                  const std::string& apiKey = "",
                  const std::string& model = "deepseek-v4-pro");
```

| 参数 | 说明 |
|------|------|
| `endpoint` | OpenAI 兼容的 chat completions URL（必填） |
| `apiKey` | API 密钥，为空时从 `OPENAI_API_KEY` 环境变量读取 |
| `model` | 模型名称，默认 "deepseek-v4-pro" |
| **返回值** | `true` = 配置成功，`false` = API Key 为空 |

### 8.4 isLLMConfigured() — 查询配置状态

```cpp
bool isLLMConfigured() const;
```

前端应在启动时和设置变更后调用此方法，以决定是否启用 AI 报告按钮。

### 8.5 generateAIReport() — AI 驱动的个性化健康报告

```cpp
std::string generateAIReport(ReportPeriod period);
```

**AI-First 策略（自动降级）**：

1. 若 LLM 服务已配置 API Key → 提取时段数据 → 组装 Prompt（RAG 上下文注入）→ 调用 AI → 返回 JSON。
2. 若 AI 调用失败（网络/超时/异常）→ 自动降级为 `generateHealthReport(period)`，返回纯文本。
3. 若 LLM 未配置 → 直接调用 `generateHealthReport(period)`，返回纯文本。

#### 8.2.1 成功时的 JSON 返回结构

AI 调用成功时返回一个结构化 JSON 字符串：

```json
{
  "risk_analysis": {
    "overall": "整体风险描述...",
    "cardiovascular": "心血管风险分析...",
    "metabolic": "代谢风险分析...",
    "alert_items": ["警示项1", "警示项2", "..."]
  },
  "action_plan": {
    "diet": ["饮食建议1", "饮食建议2"],
    "exercise": ["运动建议1", "运动建议2"],
    "lifestyle": ["生活方式建议1"],
    "monitoring": ["监测建议1", "监测建议2"]
  },
  "conclusion": "总结...",
  "follow_up_prompt": "💬 您可能还想了解：...",
  "disclaimer": "免责声明文本..."
}
```

#### 8.2.2 JSON 解析示例

```cpp
#include <QJsonDocument>
#include <QJsonObject>

void parseAIReport(const std::string& raw) {
    QJsonDocument doc = QJsonDocument::fromJson(
        QString::fromStdString(raw).toUtf8()
    );

    if (!doc.isObject()) {
        // 不是 JSON → 已降级为本地报告文本
        displayPlainText(raw);
        return;
    }

    QJsonObject root = doc.object();

    // 解析风险分析
    auto riskAnalysis = root["risk_analysis"].toObject();
    QString overall = riskAnalysis["overall"].toString();
    auto alerts = riskAnalysis["alert_items"].toArray();

    // 解析行动计划
    auto actionPlan = root["action_plan"].toObject();
    auto dietList = actionPlan["diet"].toArray();
    auto exerciseList = actionPlan["exercise"].toArray();

    // 解析其他字段
    QString conclusion = root["conclusion"].toString();
    QString followUp = root["follow_up_prompt"].toString();
    QString disclaimer = root["disclaimer"].toString();

    // 在 UI 中展示...
}
```

### 8.3 askFollowUp() — AI 追问

```cpp
std::string askFollowUp(const std::string& userQuestion);
```

- **前提**：必须先调用 `generateAIReport()` 生成报告，该方法会在后端缓存健康数据上下文。如果尚未生成报告或 LLM 未配置，返回提示信息。
- **功能**：用户可以在查看 AI 报告后追问具体问题，如饮食建议、运动方案、用药咨询等。
- **返回**：AI 回复文本（自由格式，非 JSON）。

**典型使用流程**：

```cpp
// 步骤 1：生成 AI 报告（缓存健康数据上下文）
std::string aiReport = mgr->generateAIReport(
    health::HealthManager::ReportPeriod::MONTHLY
);
// 展示报告...

// 步骤 2：用户追问
std::string answer1 = mgr->askFollowUp("我应该怎么降低空腹血糖？");
// 展示回答...

std::string answer2 = mgr->askFollowUp("建议的饮食方案具体怎么执行？");
// 展示回答...
```

---

## 9. 统计摘要

```cpp
std::string getStatistics(HealthRecordType type) const;
```

- **返回**：包含 min/max/avg 等统计信息的纯文本字符串。
- **支持的类型**：`VITALS`、`LAB_TEST`、`BP`（`HISTORY` 不参与统计）。

**示例**：

```cpp
// 查看血压统计
std::string bpStats = mgr->getStatistics(health::HealthRecordType::BP);

// 查看检验指标统计
std::string labStats = mgr->getStatistics(health::HealthRecordType::LAB_TEST);
```

---

## 10. 时间处理

### 10.1 TimePoint 类型

```cpp
using TimePoint = std::chrono::system_clock::time_point;
```

### 10.2 常用时间操作

```cpp
#include <chrono>

namespace sc = std::chrono;

// 获取当前时间
auto now = sc::system_clock::now();

// 过去的时间点
auto weekAgo  = now - sc::hours(24 * 7);    // 1 周前
auto monthAgo = now - sc::hours(24 * 30);   // 30 天前
auto yearAgo  = now - sc::hours(24 * 365);  // 1 年前

// std::nullopt 表示"无限制"
auto allRecords = mgr->getVitalsRecords(std::nullopt, std::nullopt);  // 全部
auto fromStart  = mgr->getVitalsRecords(std::nullopt, now);           // 截至现在的全部
auto toEnd      = mgr->getVitalsRecords(weekAgo, std::nullopt);       // 从1周前至今
```

### 10.3 ISO 8601 时间戳字符串

`TrendPoint::timestamp` 使用 ISO 8601 格式字符串，如 `"2024-06-15T08:00:00Z"`。在 Qt 中解析：

```cpp
#include <QDateTime>

QString isoStr = QString::fromStdString(pt.timestamp);
QDateTime dt = QDateTime::fromString(isoStr, Qt::ISODate);
```

---

## 11. Qt 集成示例

### 11.1 集成架构建议

推荐将 `HealthManager` 实例放在一个单例或主 App 类中：

```cpp
// HealthApp.h
#pragma once
#include <QObject>
#include <memory>
#include "HealthManager.h"

class HealthApp : public QObject {
    Q_OBJECT
public:
    static HealthApp* instance();

    health::HealthManager* manager() { return healthMgr_.get(); }

private:
    HealthApp();
    std::unique_ptr<health::HealthManager> healthMgr_;
};
```

### 11.2 QLineSeries 绘制趋势图

```cpp
#include <QLineSeries>
#include <QDateTimeAxis>
#include <QValueAxis>
#include <QChart>
#include <QChartView>

void drawTrendChart(QChartView* chartView, health::HealthManager* mgr) {
    using namespace sc = std::chrono;

    auto now = sc::system_clock::now();
    auto monthAgo = now - sc::hours(24 * 30);

    auto report = mgr->analyzeTrendReport(
        health::HealthRecordType::BP, monthAgo, now
    );

    auto* chart = new QChart();
    chart->setTitle(QString::fromStdString(report.title));

    for (const auto& metric : report.metrics) {
        auto* series = new QLineSeries();
        series->setName(QString::fromStdString(metric.metricName));

        for (const auto& pt : metric.dataPoints) {
            QDateTime dt = QDateTime::fromString(
                QString::fromStdString(pt.timestamp), Qt::ISODate
            );
            series->append(dt.toMSecsSinceEpoch(), pt.value);
        }

        chart->addSeries(series);
    }

    // 配置坐标轴
    auto* axisX = new QDateTimeAxis();
    axisX->setFormat("MM-dd");
    axisX->setTitleText("日期");
    chart->addAxis(axisX, Qt::AlignBottom);

    auto* axisY = new QValueAxis();
    axisY->setTitleText("mmHg");
    chart->addAxis(axisY, Qt::AlignLeft);

    for (auto* series : chart->series()) {
        auto* lineSeries = static_cast<QLineSeries*>(series);
        lineSeries->attachAxis(axisX);
        lineSeries->attachAxis(axisY);
    }

    chartView->setChart(chart);
}
```

### 11.3 QTreeView / QTableView 展示记录列表

```cpp
#include <QStandardItemModel>

void populateVitalsTable(QTableView* tableView, health::HealthManager* mgr) {
    auto records = mgr->getVitalsRecords(std::nullopt, std::nullopt);
    auto* model = new QStandardItemModel(records.size(), 6, tableView);

    model->setHorizontalHeaderLabels({
        "日期", "心率", "步数", "睡眠(h)", "体重(kg)", "身高(cm)"
    });

    for (size_t row = 0; row < records.size(); ++row) {
        const auto& r = records[row];

        auto toTimeStr = [](const health::TimePoint& tp) -> QString {
            auto time_t = std::chrono::system_clock::to_time_t(tp);
            return QDateTime::fromSecsSinceEpoch(time_t).toString("yyyy-MM-dd hh:mm");
        };

        auto setVal = [&](int col, const auto& opt) {
            using T = std::decay_t<decltype(opt)>;
            if constexpr (std::is_same_v<T, std::optional<double>>) {
                model->setItem(row, col,
                    new QStandardItem(opt.has_value()
                        ? QString::number(opt.value(), 'f', 1)
                        : "-"));
            } else if constexpr (std::is_same_v<T, std::optional<int>>) {
                model->setItem(row, col,
                    new QStandardItem(opt.has_value()
                        ? QString::number(opt.value())
                        : "-"));
            }
        };

        model->setItem(row, 0, new QStandardItem(toTimeStr(r.timestamp)));
        setVal(1, r.heartRate);
        setVal(2, r.steps);
        setVal(3, r.sleepHours);
        setVal(4, r.weightKg);
        setVal(5, r.heightCm);
    }

    tableView->setModel(model);
}
```

### 11.4 完整使用示例：主窗口集成

```cpp
#include <QMainWindow>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include "HealthManager.h"

class HealthDashboard : public QMainWindow {
    Q_OBJECT

public:
    HealthDashboard(QWidget* parent = nullptr)
        : QMainWindow(parent)
        , mgr_(health::createHealthManager())
    {
        auto* central = new QWidget(this);
        auto* layout = new QVBoxLayout(central);

        reportDisplay_ = new QTextEdit(this);
        reportDisplay_->setReadOnly(true);

        auto* btnWeekly = new QPushButton("生成周报", this);
        auto* btnAI = new QPushButton("AI 月报", this);
        auto* btnASCVD = new QPushButton("ASCVD 评估", this);

        layout->addWidget(reportDisplay_);
        layout->addWidget(btnWeekly);
        layout->addWidget(btnAI);
        layout->addWidget(btnASCVD);

        setCentralWidget(central);

        connect(btnWeekly, &QPushButton::clicked, this, [this]() {
            auto report = mgr_->generateHealthReport(
                health::HealthManager::ReportPeriod::WEEKLY
            );
            reportDisplay_->setPlainText(QString::fromStdString(report));
        });

        connect(btnAI, &QPushButton::clicked, this, [this]() {
            auto report = mgr_->generateAIReport(
                health::HealthManager::ReportPeriod::MONTHLY
            );
            reportDisplay_->setPlainText(QString::fromStdString(report));
        });

        connect(btnASCVD, &QPushButton::clicked, this, [this]() {
            double score = mgr_->calculateASCVDScore();
            QString msg = QString("10年ASCVD风险: %1%\n风险等级: %2")
                .arg(score, 0, 'f', 1)
                .arg(QString::fromStdString(
                    health::ASCVDCalculator::getRiskCategory(score)
                ));
            reportDisplay_->setPlainText(msg);
        });
    }

private:
    std::unique_ptr<health::HealthManager> mgr_;
    QTextEdit* reportDisplay_;
};
```

---

## 12. 错误处理最佳实践

### 12.1 CRUD 操作

所有 add/update/delete 方法返回 `bool`，true 表示成功。应在每次调用后检查返回值：

```cpp
if (!mgr->addVitalsRecord(record)) {
    // 写入失败，提示用户
    QMessageBox::warning(this, "保存失败", "体征记录保存失败，请重试。");
    return;
}
```

### 12.2 查询操作的 Optional 返回值

`getUserProfile()` 返回 `std::optional<UserProfile>`，必须先检查 `has_value()`：

```cpp
auto profileOpt = mgr->getUserProfile();
if (!profileOpt.has_value()) {
    QMessageBox::information(this, "提示", "请先填写用户档案。");
    return;
}
auto& profile = profileOpt.value();
// 使用 profile 的字段...
```

### 12.3 AI 功能的 JSON/降级判断

`generateAIReport()` 可能返回 JSON 或纯文本。通过尝试解析 JSON 来判断：

```cpp
std::string raw = mgr->generateAIReport(period);
QJsonDocument doc = QJsonDocument::fromJson(
    QString::fromStdString(raw).toUtf8()
);
if (doc.isObject()) {
    // AI 调用成功，按 JSON 结构展示
    displayAIReport(doc.object());
} else {
    // 降级为本地报告，按纯文本展示
    displayPlainText(raw);
}
```

### 12.4 空结果处理

- `analyzeTrendReport` 在无数据时返回 `isEmpty() == true`，务必先检查。
- `getXxxRecords` 返回空 vector 表示无匹配数据。

### 12.5 常见陷阱

| 陷阱 | 说明 |
|------|------|
| 忘记设置 `recordType` | 创建记录时必须设置 `recordType`，否则后端无法正确存储和分类 |
| `askFollowUp` 未先生成报告 | 必须先调用 `generateAIReport()` 缓存上下文 |
| `MedicalHistoryRecord` 不使用时间范围 | `getMedicalHistoryRecords()` 无参数，始终返回全部 |
| 未导出 `OPENAI_API_KEY` | AI 功能静默降级为本地报告，不会报错 |

---

## 13. 完整接口速查表

### 工厂函数

| 函数 | 签名 | 返回值 |
|------|------|--------|
| 创建实例 | `createHealthManager()` | `std::unique_ptr<HealthManager>` |

### CRUD — 体征记录

| 方法 | 签名 | 返回值 |
|------|------|--------|
| 添加 | `addVitalsRecord(const VitalsRecord&)` | `bool` |
| 更新 | `updateVitalsRecord(const VitalsRecord&)` | `bool` |
| 删除 | `deleteVitalsRecord(const std::string& id)` | `bool` |
| 查询 | `getVitalsRecords(optional<TimePoint> from, optional<TimePoint> to)` | `vector<VitalsRecord>` |

### CRUD — 检验记录

| 方法 | 签名 | 返回值 |
|------|------|--------|
| 添加 | `addLabTestRecord(const LabTestRecord&)` | `bool` |
| 更新 | `updateLabTestRecord(const LabTestRecord&)` | `bool` |
| 删除 | `deleteLabTestRecord(const std::string& id)` | `bool` |
| 查询 | `getLabTestRecords(optional<TimePoint> from, optional<TimePoint> to)` | `vector<LabTestRecord>` |

### CRUD — 血压记录

| 方法 | 签名 | 返回值 |
|------|------|--------|
| 添加 | `addBloodPressureRecord(const BloodPressureRecord&)` | `bool` |
| 更新 | `updateBloodPressureRecord(const BloodPressureRecord&)` | `bool` |
| 删除 | `deleteBloodPressureRecord(const std::string& id)` | `bool` |
| 查询 | `getBloodPressureRecords(optional<TimePoint> from, optional<TimePoint> to)` | `vector<BloodPressureRecord>` |

### CRUD — 病历摘要

| 方法 | 签名 | 返回值 |
|------|------|--------|
| 添加 | `addMedicalHistoryRecord(const MedicalHistoryRecord&)` | `bool` |
| 更新 | `updateMedicalHistoryRecord(const MedicalHistoryRecord&)` | `bool` |
| 删除 | `deleteMedicalHistoryRecord(const std::string& id)` | `bool` |
| 查询 | `getMedicalHistoryRecords()` | `vector<MedicalHistoryRecord>` |

### CRUD — 用户档案

| 方法 | 签名 | 返回值 |
|------|------|--------|
| 保存 | `saveUserProfile(const UserProfile&)` | `bool` |
| 查询 | `getUserProfile()` | `optional<UserProfile>` |
| 删除 | `deleteUserProfile(const std::string& id)` | `bool` |

### 风险评估

| 方法 | 返回值 | 说明 |
|------|--------|------|
| `calculateASCVDScore()` | `double` | 10年 ASCVD 风险百分比 |
| `calculateBMI()` | `double` | BMI 数值 |
| `getBMICategory()` | `string` | BMI 中文分级 |
| `calculateTyGIndex()` | `MetabolicResult` | 胰岛素抵抗评估 |
| `calculateCDRS()` | `MetabolicResult` | 糖尿病风险评分 |

### 趋势分析

| 方法 | 返回值 | 说明 |
|------|--------|------|
| `analyzeTrend(type, from, to)` | `TrendResult` | 旧版，单一指标 |
| `analyzeTrendReport(type, from, to)` | `TrendReport` | 新版，全部指标+时间序列 |

### 健康报告

| 方法 | 返回值 | 说明 |
|------|--------|------|
| `generateHealthReport()` | `string` | 健康快照（最新值） |
| `generateHealthReport(period)` | `string` | 周期性报告（WEEKLY/MONTHLY） |
| `generateAIReport(period)` | `string` | AI 报告（JSON 或降级文本） |

### LLM 配置

| 方法 | 返回值 | 说明 |
|------|--------|------|
| `configureLLM(endpoint, apiKey, model)` | `bool` | 配置 LLM API 连接 |
| `isLLMConfigured()` | `bool` | 查询是否已配置 |

### AI 功能

| 方法 | 返回值 | 前提条件 |
|------|--------|----------|
| `generateAIReport(period)` | `string` | LLM 需已配置（否则降级） |
| `askFollowUp(question)` | `string` | 必须先调用 `generateAIReport()` |

### 统计

| 方法 | 返回值 | 说明 |
|------|--------|------|
| `getStatistics(type)` | `string` | min/max/avg 纯文本摘要 |

---

> **文档生成日期**: 2026-06-19
> **所依据的头文件**: `backend/include/HealthManager.h`
> **项目仓库**: [OmniHealth_Profiler](https://github.com/Felis1204/OmniHealth_Profiler)
