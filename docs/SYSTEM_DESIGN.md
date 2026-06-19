# OmniHealth Profiler - 系统架构设计

> 最后更新: 2026-06-19 | 开发阶段: Phase 2

---

## 1. 核心架构模式 (MVC)

```
┌────────────────────────────────────────────────────────────────────┐
│                        Frontend (View)                             │
│  Qt6 Widgets: widget.ui + AddData.ui                              │
│  ├── widget.h / widget.cpp          — 主窗口，数据展示与操作入口     │
│  └── AddData.h / AddData.cpp        — 数据录入对话框                │
│  权限: 仅通过 HealthManager.h 契约调用后端                           │
│  持有: std::unique_ptr<HealthManager>                              │
└─────────────────────────────┬──────────────────────────────────────┘
                              │  #include "HealthManager.h"
                              │
┌─────────────────────────────▼──────────────────────────────────────┐
│                       Backend (Controller + Model + Service)        │
│                                                                     │
│  ┌─────────────────┐  ┌──────────────┐  ┌─────────────────────┐   │
│  │  HealthManager   │  │  DataAccess   │  │  ASCVDCalculator    │   │
│  │  (Controller)   │  │  (SQLite3)   │  │  (China-PAR)        │   │
│  │  29 个虚方法     │  │  6 个方法     │  │  ASCVDParams(12字段) │   │
│  └────────┬────────┘  └──────┬───────┘  └─────────────────────┘   │
│           │                  │                                      │
│  ┌────────▼────────┐         │         ┌─────────────────────┐    │
│  │  Models/ (6个)   │         │         │ MetabolicCalculator │    │
│  │  HealthRecord   │         │         │  TyG + CDRS         │    │
│  │  VitalsRecord   │         │         └─────────────────────┘    │
│  │  LabTestRecord  │         │                                      │
│  │  BPRecord       │         │         ┌─────────────────────┐    │
│  │  MedicalHistory │         │         │  LLMService          │    │
│  │  UserProfile    │         │         │  cpp-httplib+DeepSeek│   │
│  └─────────────────┘         │         └─────────────────────┘    │
│                              │                                      │
│  ┌───────────────────────────▼──────────────────────────────┐     │
│  │                   SQLite3 Database                        │     │
│  │  user_profile / vitals_records / lab_test_records /       │     │
│  │  blood_pressure_records / medical_history_records         │     │
│  │  WAL mode + foreign_keys=ON                               │     │
│  └──────────────────────────────────────────────────────────┘     │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────┐     │
│  │  PlatformCompat  (跨平台适配层)                             │     │
│  │  gmtimeCompat / timegmCompat  (macOS/Linux/Windows)       │     │
│  └──────────────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 2. 分层职责

| 层级 | 模块 | 职责 | 技术栈 | 状态 |
|------|------|------|--------|------|
| **View** | frontend/ | Qt6 窗口渲染、用户交互、数据录入表单 | Qt6 Widgets | ✅ |
| **Controller** | HealthManager | API 协调、业务流程编排、算法调度、AI 报告生成 | C++17 | ✅ |
| **Model** | Models/ | 数据模型定义（5个派生 struct + 基类 + 独立 UserProfile） | C++17 | ✅ |
| **Persistence** | DataAccess | SQLite CRUD、建表、索引、版本迁移 | SQLite3 C API | ✅ |
| **Algorithm** | ASCVDCalculator | China-PAR 10年心血管风险评估 | C++17 `<cmath>` | ✅ |
| **Algorithm** | MetabolicCalculator | TyG 胰岛素抵抗筛查 + CDRS 糖尿病风险评分 | C++17 `<cmath>` | ✅ |
| **Service** | LLMService | DeepSeek API 调用、Prompt 模板化、数据脱敏、JSON 约束 | cpp-httplib | ✅ |
| **Cross-platform** | PlatformCompat | gmtime/timegm 跨平台封装 + 编译器/平台检测宏 | inline 函数 | ✅ |

---

## 3. 架构原则

1. **依赖方向**：View → Controller → Model / Persistence / Algorithm → Service（单向依赖，无循环）
2. **契约驱动**：Frontend 与 Backend 之间仅通过 `HealthManager.h` 纯虚接口通信，前端只能持有 `std::unique_ptr<HealthManager>`
3. **PIMPL 模式**：
   - `HealthManagerImpl`（~1500 行）实现 `HealthManager`，隐藏在 .cpp 中
   - `DataAccessImpl`（~560 行）实现 `DataAccess`，持有 `sqlite3* db_`
   - `LLMServiceImpl` 实现 `LLMService`，持有 httplib::Client 和配置参数
4. **纯计算分离**：`ASCVDCalculator` 和 `MetabolicCalculator` 为纯静态方法类，不依赖数据库，通过参数结构体传值，便于单元测试
5. **序列化解耦**：Struct 与 JSON 双向转换在 `HealthManager.cpp` 内部实现，DataAccess 仅操作 JSON 字符串

---

## 4. 数据模型

### 4.1 继承体系

```
namespace health {

HealthRecord (基类)
├── VitalsRecord        —— 基础体征（心率/步数/睡眠/体重/身高/腰围）
├── LabTestRecord       —— 临床检验（血糖/血脂/尿酸）
├── BloodPressureRecord —— 血压专项（收缩压/舒张压）
└── MedicalHistoryRecord —— 病历摘要（自由文本，参与 AI 分析但不参与趋势计算）

UserProfile (独立模型，非 HealthRecord 派生)
  ├── 基本信息: id, name, birthDate(ISO8601), gender("MALE"/"FEMALE")
  ├── 风险因子: smokingStatus("NEVER"/"FORMER"/"CURRENT"), hasDiabetes
  └── China-PAR: region("NORTH"/"SOUTH"), urbanRural("URBAN"/"RURAL"), familyHistoryASCVD
}
```

### 4.2 基类定义

```cpp
namespace health {

using TimePoint = std::chrono::system_clock::time_point;

enum class HealthRecordType {
    VITALS,     // 基础体征
    LAB_TEST,   // 临床检验
    BP,         // 血压
    HISTORY     // 病历摘要
};

struct HealthRecord {
    std::string                     id;          // UUID 唯一标识
    HealthRecordType                recordType;  // 记录类型
    TimePoint                       timestamp;   // 采集/录入时间
    std::optional<std::string>      source;      // 数据来源（手动/设备型号）
    std::optional<std::string>      note;        // 备注
};

} // namespace health
```

### 4.3 派生结构体定义

```cpp
// 基础体征记录
struct VitalsRecord : public HealthRecord {
    std::optional<double>   heartRate;    // 心率 (bpm)
    std::optional<int>      steps;        // 步数
    std::optional<double>   sleepHours;   // 睡眠时长 (小时)
    std::optional<double>   weightKg;     // 体重 (kg)
    std::optional<double>   heightCm;     // 身高 (cm, 用于 BMI)
    std::optional<double>   waistCm;      // 腰围 (cm, 用于代谢综合征/ASCVD)
};

// 临床检验记录
struct LabTestRecord : public HealthRecord {
    std::optional<double>   fastingGlucose;     // 空腹血糖 (mmol/L)
    std::optional<double>   totalCholesterol;   // 总胆固醇 (mmol/L)
    std::optional<double>   ldlC;               // 低密度脂蛋白 (mmol/L)
    std::optional<double>   hdlC;               // 高密度脂蛋白 (mmol/L)
    std::optional<double>   triglycerides;      // 甘油三酯 (mmol/L)
    std::optional<double>   uricAcid;           // 血尿酸 (µmol/L)
};

// 血压专项记录
struct BloodPressureRecord : public HealthRecord {
    std::optional<int>      systolic;     // 收缩压 (mmHg)
    std::optional<int>      diastolic;    // 舒张压 (mmHg)
};

// 病历摘要记录（自由文本，不参与趋势计算）
struct MedicalHistoryRecord : public HealthRecord {
    std::string             category;     // "既往病史"/"手术史"/"过敏史"/"家族史"/"用药史"/"其他"
    std::string             content;      // 自由文本内容
};

// 用户档案（独立模型）
struct UserProfile {
    std::string                     id;                  // UUID 主键
    std::string                     name;                // 姓名（NOT NULL）
    std::optional<std::string>      birthDate;           // 出生日期 ISO 8601
    std::optional<std::string>      gender;              // "MALE" / "FEMALE"
    std::optional<std::string>      smokingStatus;       // "NEVER" / "FORMER" / "CURRENT"
    std::optional<std::string>      region;              // "NORTH" / "SOUTH"
    std::optional<std::string>      urbanRural;          // "URBAN" / "RURAL"
    std::optional<bool>             familyHistoryASCVD;  // 早发 ASCVD 家族史（男<55/女<65）
    std::optional<bool>             hasDiabetes;         // 确诊糖尿病（显式标注，优先级高于血糖判定）
};
```

### 4.4 趋势分析与报告数据结构

```cpp
// --- 旧版趋势结果（向后兼容）---
struct TrendResult {
    double average;   // 平均值
    double min;       // 最小值
    double max;       // 最大值
    double median;    // 中位数
    double slope;     // 线性回归斜率（正=上升，负=下降）
};

// --- 新版趋势报告 ---
struct TrendPoint {
    std::string timestamp;   // ISO 8601 字符串，如 "2024-06-15T08:00:00Z"
    double value;            // 指标数值
};

struct MetricTrend {
    std::string             metricName;    // 指标中文名，如 "收缩压"
    std::string             metricKey;     // 指标键，如 "systolic"（前端排序/匹配用）
    std::string             unit;          // 单位，如 "mmHg"、"kg"、"mmol/L"
    std::vector<TrendPoint> dataPoints;    // 时间-数值数据点序列（按时间升序）

    // 统计摘要（图表下方展示）
    double average = 0.0;
    double min     = 0.0;
    double max     = 0.0;
    double median  = 0.0;
    double slope   = 0.0;    // 线性回归斜率
    int    count   = 0;      // 数据点个数
};

struct TrendReport {
    HealthRecordType        recordType;   // VITALS / LAB_TEST / BP
    std::string             title;        // 如 "体征指标趋势分析"
    std::string             periodLabel;  // 如 "2024-06-01 至 2024-06-18"
    std::vector<MetricTrend> metrics;     // 每个可量化指标一条折线

    bool isEmpty() const { return metrics.empty(); }
};

// --- 报告周期枚举 ---
enum class ReportPeriod {
    WEEKLY,   // 近 7 天
    MONTHLY   // 近 30 天
};
```

---

## 5. 核心类设计

### 5.1 HealthManager —— 对外统一接口（29 个纯虚方法）

```cpp
class HealthManager {
public:
    HealthManager() = default;
    virtual ~HealthManager() = default;

    // ---- CRUD (16 个方法) ----
    virtual bool addVitalsRecord(const VitalsRecord& record) = 0;
    virtual bool addLabTestRecord(const LabTestRecord& record) = 0;
    virtual bool addBloodPressureRecord(const BloodPressureRecord& record) = 0;
    virtual bool addMedicalHistoryRecord(const MedicalHistoryRecord& record) = 0;

    virtual bool updateVitalsRecord(const VitalsRecord& record) = 0;
    virtual bool updateLabTestRecord(const LabTestRecord& record) = 0;
    virtual bool updateBloodPressureRecord(const BloodPressureRecord& record) = 0;
    virtual bool updateMedicalHistoryRecord(const MedicalHistoryRecord& record) = 0;

    virtual bool deleteVitalsRecord(const std::string& id) = 0;
    virtual bool deleteLabTestRecord(const std::string& id) = 0;
    virtual bool deleteBloodPressureRecord(const std::string& id) = 0;
    virtual bool deleteMedicalHistoryRecord(const std::string& id) = 0;

    virtual std::vector<VitalsRecord> getVitalsRecords(
        std::optional<TimePoint> from, std::optional<TimePoint> to) const = 0;
    virtual std::vector<BloodPressureRecord> getBloodPressureRecords(
        std::optional<TimePoint> from, std::optional<TimePoint> to) const = 0;
    virtual std::vector<LabTestRecord> getLabTestRecords(
        std::optional<TimePoint> from, std::optional<TimePoint> to) const = 0;
    virtual std::vector<MedicalHistoryRecord> getMedicalHistoryRecords() const = 0;

    // ---- UserProfile (3 个方法) ----
    virtual bool saveUserProfile(const UserProfile& profile) = 0;        // Upsert 语义
    virtual std::optional<UserProfile> getUserProfile() const = 0;       // LIMIT 1
    virtual bool deleteUserProfile(const std::string& id) = 0;

    // ---- 风险计算 (5 个方法) ----
    virtual double calculateASCVDScore() const = 0;         // China-PAR 10年 ASCVD 风险
    virtual double calculateBMI() const = 0;                 // 体重(kg) / 身高(m)^2
    virtual std::string getBMICategory() const = 0;          // 五级: 偏瘦/正常/超重/肥胖
    virtual MetabolicResult calculateTyGIndex() const = 0;   // 甘油三酯-葡萄糖指数
    virtual MetabolicResult calculateCDRS() const = 0;       // 中国糖尿病风险评分

    // ---- 趋势分析 (2 个方法) ----
    virtual TrendResult analyzeTrend(HealthRecordType type,
        TimePoint from, TimePoint to) const = 0;              // 旧版（单指标统计）
    virtual TrendReport analyzeTrendReport(HealthRecordType type,
        std::optional<TimePoint> from,
        std::optional<TimePoint> to) const = 0;               // 新版（全指标数据点+摘要）

    // ---- 健康报告 (3 个方法) ----
    virtual std::string generateHealthReport() const = 0;                       // 快照报告
    virtual std::string generateHealthReport(ReportPeriod period) const = 0;    // 周期报告
    virtual std::string generateAIReport(ReportPeriod period) = 0;              // AI 驱动（含降级）

    // ---- AI 咨询 (1 个方法) ----
    virtual std::string askFollowUp(const std::string& userQuestion) = 0;

    // ---- 统计摘要 (1 个方法) ----
    virtual std::string getStatistics(HealthRecordType type) const = 0;
};
```

**工厂函数：** `std::unique_ptr<HealthManager> createHealthManager();`

**内部实现 (HealthManagerImpl)**：
- 持有 `std::unique_ptr<DataAccess> dataAccess_` 和 `std::unique_ptr<LLMService> llmService_`
- 追问上下文缓存：`std::string lastHealthContext_` + `std::string lastPeriodLabel_`
- 辅助方法：`buildTimeRangeQuery(table, selectCols, from, to)`, `getAverageSystolicBP(count)`
- 序列化/反序列化工具函数（静态 file 作用域函数）：`vitalsToJson`, `jsonToVitals`, `labTestToJson`, `jsonToLabTest`, `bpToJson`, `jsonToBp`, `medicalHistoryToJson`, `jsonToMedicalHistory`, `userProfileToJson`, `jsonToUserProfile`
- 统计工具函数：`median(v)`, `linearSlope(y)`, `calculateAge(birthDate)`

### 5.2 DataAccess —— 数据访问层（6 个纯虚方法）

```cpp
class DataAccess {
public:
    DataAccess() = default;
    virtual ~DataAccess() = default;

    virtual bool initialize(const std::string& dbPath) = 0;
    virtual bool insertRecord(const std::string& table, const std::string& jsonValue) = 0;
    virtual bool deleteRecord(const std::string& table, const std::string& id) = 0;
    virtual bool updateRecord(const std::string& table, const std::string& id, const std::string& jsonValue) = 0;
    virtual std::string queryRecords(const std::string& sql) = 0;
    virtual bool executeMigration(int version) = 0;
};
```

**工厂函数：** `std::unique_ptr<DataAccess> createDataAccess();`

**内部实现 (DataAccessImpl)**：
- 持有 `sqlite3* db_`
- 安全策略：表名白名单 + 列名映射 + 预编译语句绑定
- 表名白名单（5 张表）：`user_profile`, `vitals_records`, `lab_test_records`, `blood_pressure_records`, `medical_history_records`
- 每张表维护独立列序，INSERT/UPDATE 时按列序绑定参数
- `initialize()` 中执行：`PRAGMA journal_mode=WAL;` + `PRAGMA foreign_keys=ON;` + `createSchema()`
- `createSchema()` 幂等建表（`CREATE TABLE IF NOT EXISTS` + `CREATE INDEX IF NOT EXISTS`）
- 查询结果以 JSON 数组字符串返回（列类型自动推断：INTEGER / FLOAT / TEXT / NULL）
- `executeMigration(version)` 当前为占位实现（TODO）

### 5.3 ASCVDCalculator —— 心血管风险算法

```cpp
struct ASCVDParams {
    int    age              = 0;
    bool   isMale           = true;
    double systolicBP       = 0.0;   // mmHg
    double fastingGlucose   = 0.0;   // mmol/L
    double totalCholesterol = 0.0;   // mmol/L（计算时转换为 mg/dL ÷0.0259）
    double hdlC             = 0.0;   // mmol/L（计算时转换为 mg/dL）
    double waistCm          = 0.0;   // cm
    bool   isCurrentSmoker  = false;
    bool   hasDiabetes      = false; // 显式诊断标记
    bool   isNorthern       = true;  // 北方=1, 南方=0
    bool   isUrban          = true;  // 城市=1, 乡村=0
    bool   hasFamilyHistory = false; // 早发 ASCVD 家族史
};

class ASCVDCalculator {
public:
    ASCVDCalculator() = delete;           // 纯静态类，禁止实例化

    static double calculateChinaPAR(const ASCVDParams& params);
    static std::string getRiskCategory(double riskPercentage);
};
```

- 参考论文：Yang X, et al. *Circulation*. 2016;134:1430-1440
- 男女双分支公式（含交互项：年龄×收缩压、年龄×腰围、年龄×吸烟 等）
- 计算流程：TC/HDL 单位换算 → ln 变换 → 系数累加 → 个体求和 → `Risk = 1 - S10^exp(Sum - MeanXB)`
- 年龄校验范围：35-74 岁
- 糖尿病自动判定补充（Glu >= 7.0 mmol/L）
- **五级风险分层**：<5.0% 低危 / 5.0%-7.4% 临界 / 7.5%-9.9% 中危 / 10.0%-19.9% 高危 / >=20.0% 极高危

### 5.4 MetabolicCalculator —— 内分泌代谢评估算法

```cpp
// --- 算法入参 ---
struct TyGParams {
    double fastingGlucose;   // mmol/L
    double triglycerides;    // mmol/L
};

struct CDRSParams {
    int    age;
    bool   isMale;
    double waistCm;          // 厘米
    bool   hasFamilyHistory; // 父母或兄弟姐妹是否有糖尿病
};

struct MetabolicResult {
    double score;            // 连续评分值
    std::string riskLevel;   // 风险定性描述
};

class MetabolicCalculator {
public:
    // TyG = ln( TG(mg/dL) × FPG(mg/dL) / 2 )，切点 >= 8.70
    static MetabolicResult calculateTyGIndex(const TyGParams& params);

    // CDRS = AgeScore(1-12) + WaistScore(cm→市尺, 1-12) + FamilyScore(1/8)
    // 男性 >= 17 或 女性 >= 14 → 高度疑似隐匿性糖尿病，建议 OGTT
    static MetabolicResult calculateCDRS(const CDRSParams& params);
};
```

- TyG 指数参考：Dicky et al. *Diabetes & Metabolic Syndrome: CRR*, 2022
- CDRS 参考：Gao et al. *Diabetic Medicine*, 2010
- 均为纯静态方法类，无状态，不依赖数据库

### 5.5 LLMService —— 大模型服务层

```cpp
class LLMService {
public:
    LLMService() = default;
    virtual ~LLMService() = default;

    // ---- 配置 ----
    virtual bool configure(const std::string& endpoint,
                           const std::string& apiKey = "",
                           const std::string& model = "deepseek-v4-pro") = 0;
    virtual bool isConfigured() const = 0;

    // ---- 对话 ----
    virtual std::string chat(const std::string& systemPrompt,
                             const std::string& userMessage) = 0;

    // ---- Prompt 组装（静态方法）----
    static std::string buildSystemPrompt(const std::string& periodLabel);
    static std::string buildHealthContextPrompt(
        const UserProfile& profile,
        const std::vector<VitalsRecord>& vitals,
        const std::vector<BloodPressureRecord>& bps,
        const std::vector<LabTestRecord>& labs,
        const std::vector<MedicalHistoryRecord>& medicalHistory,
        double bmi, const std::string& bmiCategory,
        double ascvd, const std::string& ascvdCategory,
        const std::string& trendSummary,
        int days, const std::string& periodLabel);

    static std::string buildFollowUpSystemPrompt(const std::string& healthContext);
    static std::string buildFollowUpUserPrompt(const std::string& userQuestion);
};
```

**工厂函数：** `std::unique_ptr<LLMService> createLLMService();`

**内部实现要点**：
- cpp-httplib 发起 HTTPS POST 到 OpenAI 兼容 API
- `configure()`: apiKey 为空时自动从环境变量 `OPENAI_API_KEY` 读取
- System Prompt: 医疗专家人设 + 强制 JSON 输出约束（`report` + `suggestions` 数组）+ 结尾追问引导
- `buildHealthContextPrompt()`: 组装脱敏健康数据（去除姓名/id），自动标记异常值（参考范围对比，如空腹血糖 >6.1 ⚠️偏高）
- 追问系统：`buildFollowUpSystemPrompt` 回传之前报告的健康上下文，`buildFollowUpUserPrompt` 包装用户问题
- 异常标记工具函数：`flagGlucose`, `flagTC`, `flagHDL`, `flagLDL`, `flagTG`, `flagUA`（基于《中国成人血脂异常防治指南》参考范围）

### 5.6 PlatformCompat —— 跨平台兼容层

```cpp
// 编译器检测宏
#define HM_COMPILER_MSVC    // _MSC_VER
#define HM_COMPILER_MINGW   // __MINGW32__ || __MINGW64__
#define HM_COMPILER_CLANG   // __clang__  || __apple_build_version__
#define HM_COMPILER_GCC     // __GNUC__

// 平台检测宏
#define HM_PLATFORM_WINDOWS // _WIN32
#define HM_PLATFORM_APPLE   // __APPLE__
#define HM_PLATFORM_LINUX   // __linux__

namespace health::platform {
    inline void gmtimeCompat(const std::time_t* t, std::tm* out);
    // macOS/Linux → gmtime_r(t, out)
    // Windows    → gmtime_s(out, t)

    inline std::time_t timegmCompat(std::tm* tm);
    // macOS/Linux → timegm(tm)       (GNU/BSD 扩展)
    // Windows    → _mkgmtime(tm)     (VS2015+ / MinGW-w64)
}
```

- 全 `inline` 实现，零编译开销
- 用于 ISO 8601 时间字符串与 `TimePoint` 之间的双向转换（`timePointToIso` / `isoToTimePoint`）

---

## 6. 数据库设计

### 6.1 表结构

```sql
-- 用户档案表（China-PAR 所需人口学参数）
CREATE TABLE IF NOT EXISTS user_profile (
    id                  TEXT PRIMARY KEY,
    name                TEXT NOT NULL,
    birth_date          TEXT,
    gender              TEXT,           -- "MALE" / "FEMALE"
    smoking_status      TEXT,           -- "NEVER" / "FORMER" / "CURRENT"
    region              TEXT,           -- "NORTH" / "SOUTH"
    urban_rural         TEXT,           -- "URBAN" / "RURAL"
    family_history_ascvd INTEGER DEFAULT 0,
    has_diabetes        INTEGER DEFAULT 0,
    created_at          TEXT DEFAULT (datetime('now'))
);

-- 基础体征记录表（含腰围，用于代谢综合征/ASCVD）
CREATE TABLE IF NOT EXISTS vitals_records (
    id              TEXT PRIMARY KEY,
    user_id         TEXT,
    timestamp       TEXT NOT NULL,
    heart_rate      REAL,
    steps           INTEGER,
    sleep_hours     REAL,
    weight_kg       REAL,
    height_cm       REAL,
    waist_cm        REAL,
    source          TEXT,
    note            TEXT,
    FOREIGN KEY (user_id) REFERENCES user_profile(id)
);

-- 临床检验记录表
CREATE TABLE IF NOT EXISTS lab_test_records (
    id                  TEXT PRIMARY KEY,
    user_id             TEXT,
    timestamp           TEXT NOT NULL,
    fasting_glucose     REAL,
    total_cholesterol   REAL,
    ldl_c               REAL,
    hdl_c               REAL,
    triglycerides       REAL,
    uric_acid           REAL,
    note                TEXT,
    FOREIGN KEY (user_id) REFERENCES user_profile(id)
);

-- 血压记录表
CREATE TABLE IF NOT EXISTS blood_pressure_records (
    id          TEXT PRIMARY KEY,
    user_id     TEXT,
    timestamp   TEXT NOT NULL,
    systolic    INTEGER,
    diastolic   INTEGER,
    note        TEXT,
    FOREIGN KEY (user_id) REFERENCES user_profile(id)
);

-- 病历摘要表（自由文本，用于 AI 个性化分析）
CREATE TABLE IF NOT EXISTS medical_history_records (
    id          TEXT PRIMARY KEY,
    user_id     TEXT,
    timestamp   TEXT NOT NULL,
    category    TEXT,           -- "既往病史"/"手术史"/"过敏史"/"家族史"/"用药史"/"其他"
    content     TEXT,
    note        TEXT,
    FOREIGN KEY (user_id) REFERENCES user_profile(id)
);
```

### 6.2 索引

```sql
CREATE INDEX IF NOT EXISTS idx_vitals_timestamp    ON vitals_records(timestamp);
CREATE INDEX IF NOT EXISTS idx_lab_timestamp       ON lab_test_records(timestamp);
CREATE INDEX IF NOT EXISTS idx_bp_timestamp        ON blood_pressure_records(timestamp);
CREATE INDEX IF NOT EXISTS idx_medical_history_timestamp ON medical_history_records(timestamp);
```

共有 5 张表，4 个时间戳索引，全部通过时间排序查询优化。

### 6.3 数据库配置

- `PRAGMA journal_mode=WAL` — 提升并发读性能，写入不阻塞读取
- `PRAGMA foreign_keys=ON` — 启用外键约束，保障引用完整性
- 数据库文件：项目运行目录下的 `omnihealth.db`

### 6.4 DataAccess 安全策略

- **表名白名单**：只允许操作 5 张已知表，拒绝任意表名
- **列名映射**：每张表预定义列序列表，INSERT/UPDATE 时按白名单列序绑定
- **预编译语句**：全部使用 `sqlite3_prepare_v2` + `sqlite3_bind_*`，防止 SQL 注入
- **JSON 解析验证**：所有 JSON 输入先解析验证，类型不匹配自动拒绝

---

## 7. 数据流设计

### 7.1 用户录入流程（以新增体征为例）

```
Frontend (AddData 对话框)
    │  用户填写表单，点击保存
    ▼
HealthManager::addVitalsRecord(record)
    │
    ├── vitalsToJson(record)     → 序列化 struct 到 JSON
    │       ├── camelCase 成员 → snake_case JSON key
    │       ├── std::optional 字段：仅含值时写入
    │       └── TimePoint → ISO 8601 字符串
    │
    ├── DataAccess::insertRecord("vitals_records", json.dump())
    │       ├── 表名白名单校验
    │       ├── JSON 解析 + 类型验证
    │       ├── 列名映射（预定义列序）
    │       ├── 构建参数化 INSERT SQL
    │       ├── sqlite3_prepare_v2 → bind → step
    │       └── finalize
    │
    └── 返回 bool (成功/失败)
```

### 7.2 数据查询流程（以获取体征为例）

```
HealthManager::getVitalsRecords(from, to)
    │
    ├── buildTimeRangeQuery("vitals_records", "*", from, to)
    │       ├── 无条件 FROM → "... WHERE timestamp >= ... AND timestamp <= ..."
    │       ├── 单条件 → 适配 nullopt
    │       └── ORDER BY timestamp ASC
    │
    ├── DataAccess::queryRecords(sql)
    │       ├── sqlite3_prepare_v2 + step 循环
    │       ├── 按列类型自动读取：INTEGER/FLOAT/TEXT/NULL → JSON
    │       └── 返回 JSON 数组字符串
    │
    ├── jsonToVitals(每个 JSON object)
    │       ├── ISO 8601 字符串 → TimePoint
    │       └── snake_case key → camelCase member
    │
    └── 返回 std::vector<VitalsRecord>
```

### 7.3 ASCVD 风险评估流程

```
HealthManager::calculateASCVDScore()
    │
    ├── getUserProfile()
    │       → 取出 age(计算自birthDate), gender, smoking, diabetes,
    │          region, urbanRural, familyHistory
    │
    ├── getAverageSystolicBP(3)
    │       → 倒序遍历 BP 记录，取最近 3 次有 systolic 的均值
    │
    ├── getLabTestRecords(nullopt, nullopt)
    │       → 取最新一条检验记录: TC, HDL-C, fastingGlucose
    │
    ├── getVitalsRecords(nullopt, nullopt)
    │       → 倒序取最新腰围 waistCm
    │
    ├── 年龄校验 (35-74)
    │
    ├── 组装 ASCVDParams (12 个字段)
    │       ├── 糖尿病自动判定补充: Glu >= 7.0 且 hasDiabetes 未显式标注
    │       ├── isNorthern 默认 true (仅 region=="SOUTH" 为 false)
    │       └── isUrban 默认 true (仅 urbanRural=="RURAL" 为 false)
    │
    └── ASCVDCalculator::calculateChinaPAR(params)
            ├── TC/HDL 单位换算 (mmol/L ÷ 0.0259 → mg/dL)
            ├── ln 变换 (totalChol, hdl, systolicBP, 交互项)
            ├── 男/女分支系数表
            ├── 个体求和 Sum = Σ(β_i × X_i)
            ├── Risk = 1 - S10^exp(Sum - MeanXB)
            └── 返回 0.0-100.0 风险百分比
```

### 7.4 AI 健康报告生成流程（AI-First + 离线降级）

```
HealthManager::generateAIReport(WEEKLY/MONTHLY)
    │
    ├── 检查 llmService_ 是否已配置
    │   └── 未配置 → 降级: return generateHealthReport(period)
    │                  （本地报告，基于时段均值）
    │
    ├── 已配置 → AI 路径:
    │   ├── 提取时段数据 (7/30 天):
    │   │   ├── getUserProfile()
    │   │   ├── getVitalsRecords(from, now)
    │   │   ├── getBloodPressureRecords(from, now)
    │   │   ├── getLabTestRecords(nullopt, nullopt)  // 全部检验
    │   │   ├── getMedicalHistoryRecords()            // 全部病历
    │   │   ├── calculateBMI() + getBMICategory()
    │   │   ├── calculateASCVDScore() + getRiskCategory()
    │   │   ├── calculateTyGIndex() + calculateCDRS()
    │   │   └── analyzeTrendReport(BP/VITALS, from, now)
    │   │
    │   ├── 组装 Prompt:
    │   │   ├── LLMService::buildSystemPrompt(periodLabel)   // 人设+JSON约束
    │   │   ├── LLMService::buildHealthContextPrompt(...)     // 脱敏数据上下文(12参数)
    │   │   └── 附加内分泌代谢评估结果 (TyG + CDRS)
    │   │
    │   ├── 缓存上下文: lastHealthContext_ + lastPeriodLabel_
    │   │
    │   └── llmService_->chat(systemPrompt, userPrompt)
    │       ├── cpp-httplib HTTPS POST → DeepSeek API
    │       ├── 成功 → JSON (含 report + suggestions)
    │       └── 失败 → 降级: return generateHealthReport(period)
    │
    └── 追问支持:
        ├── askFollowUp(userQuestion)
        │   ├── 检查 llmService_->isConfigured()
        │   └── 检查 lastHealthContext_ 非空
        ├── buildFollowUpSystemPrompt(lastHealthContext_)
        └── llmService_->chat(...)
```

### 7.5 趋势分析流程

```
HealthManager::analyzeTrendReport(type, from, to)
    │
    ├── getMetricDefinitions(type)
    │   ├── VITALS → 6 个指标 (心率/步数/睡眠/体重/身高/腰围)
    │   ├── LAB_TEST → 6 个指标 (空腹血糖/TC/LDL-C/HDL-C/TG/尿酸)
    │   ├── BP → 2 个指标 (收缩压/舒张压)
    │   └── HISTORY → 空 (无量化指标)
    │
    ├── 对每个指标:
    │   ├── 查询: SELECT timestamp, {col} FROM {table}
    │   │         WHERE {col} IS NOT NULL [AND timestamp范围]
    │   │         ORDER BY timestamp ASC
    │   │
    │   ├── 解析 {timestamp, value} 对
    │   │
    │   └── 计算统计:
    │       ├── average: Σvalues / n
    │       ├── min/max: std::min_element / max_element
    │       ├── median: nth_element
    │       └── slope: 最小二乘线性回归
    │
    └── 组装 TrendReport → 返回
```

---

## 8. 项目目录结构

```
Health_Manager/
├── CMakeLists.txt                          # 根 CMake (C++17, CMP 最低3.20)
├── CLAUDE.md                               # AI 代码生成规约
├── .gitignore
├── docs/
│   ├── PRD.md                              # 产品需求文档
│   └── SYSTEM_DESIGN.md                    # 本文件 —— 系统架构设计
│
├── backend/                                # 后端（Model + Controller + Persistence + Service）
│   ├── CMakeLists.txt                      # 静态库: health_backend
│   │                                       # 依赖: SQLite3 + nlohmann/json + cpp-httplib
│   │
│   ├── include/
│   │   ├── HealthManager.h                 # 控制器接口 (29 个虚方法 + 7 个数据结构)
│   │   ├── DataAccess.h                    # 数据访问层接口 (6 个虚方法)
│   │   ├── LLMService.h                    # LLM 服务接口 (4 个虚方法 + 4 个静态 Prompt)
│   │   ├── ASCVDCalculator.h               # China-PAR 算法 (ASCVDParams + 计算器)
│   │   ├── MetabolicCalculator.h           # 代谢评估算法 (TyG + CDRS + 3 个 struct)
│   │   ├── PlatformCompat.h                # 跨平台兼容层 (gmtime/timegm + 宏)
│   │   │
│   │   └── Models/                         # 数据模型 (6 个 struct)
│   │       ├── HealthRecord.h              # 基类 + TimePoint + HealthRecordType
│   │       ├── VitalsRecord.h              # 体征 (6 个可选指标字段)
│   │       ├── LabTestRecord.h             # 临床检验 (6 个可选指标字段)
│   │       ├── BloodPressureRecord.h       # 血压 (2 个可选指标字段)
│   │       ├── MedicalHistoryRecord.h      # 病历摘要 (category + content)
│   │       └── UserProfile.h               # 用户档案 (8 个可选字段 + 2 个必填)
│   │
│   └── src/
│       ├── HealthManager.cpp               # HealthManagerImpl (~1500 行)
│       │                                    #   CRUD + 风险评估 + 趋势 + 报告 + AI + 追问
│       ├── DataAccess.cpp                   # DataAccessImpl (~560 行)
│       │                                    #   建表 5 张 + 4 索引 + 白名单 + 预编译语句
│       ├── LLMService.cpp                   # LLM 服务实现 (cpp-httplib HTTPS + Prompt 模板)
│       ├── ASCVDCalculator.cpp              # China-PAR 实现 (~170 行)
│       └── MetabolicCalculator.cpp          # TyG + CDRS 实现
│
├── frontend/                               # 前端（View）
│   ├── CMakeLists.txt                      # 可执行文件: Health_Manager_App
│   │                                       # 依赖: health_backend + Qt6::Widgets
│   │
│   ├── Forms/
│   │   ├── widget.ui                       # 主窗口 UI 设计
│   │   └── AddData.ui                      # 数据录入对话框 UI 设计
│   │
│   ├── include/
│   │   ├── widget.h                        # 主窗口头文件 (Q_OBJECT)
│   │   └── AddData.h                       # 数据录入对话框头文件 (Q_OBJECT)
│   │
│   └── src/
│       ├── main.cpp                        # 应用入口 + 初始化 HealthManager
│       ├── widget.cpp                      # 主窗口实现
│       └── AddData.cpp                     # 数据录入对话框实现
│
├── tests/                                  # 测试目录
│   └── CMakeLists.txt
│
└── build/                                  # 构建输出 (gitignored)
    └── _deps/                              # FetchContent 下载的依赖缓存
        ├── httplib-src/                    # cpp-httplib v0.18.3
        └── json-src/                       # nlohmann/json v3.11.3
```

---

## 9. 依赖管理

| 依赖 | 版本 | 集成方式 | 用途 |
|------|------|---------|------|
| SQLite3 | 3.53.2+ | Homebrew + `find_package` | 本地数据持久化 |
| nlohmann/json | v3.11.3 | `FetchContent` (GitHub) | JSON 序列化/反序列化/API 交互 |
| cpp-httplib | v0.18.3 | `FetchContent` (GitHub) | LLM API HTTPS 请求 |
| Qt6 | 6.x | `find_package(Qt6 COMPONENTS Widgets)` | 桌面 UI 框架 |

**平台特定依赖**：
- macOS：`Security.framework` + `CoreFoundation.framework`（HTTPS 支持）
- Windows：SQLite3 支持 vcpkg / MSYS2 / Qt MinGW 工具链三种查找策略
- Linux：需 OpenSSL（cpp-httplib HTTPS 依赖）

---

## 10. API 调用契约

### 10.1 序列化契约

所有 struct 与 SQLite 之间的序列化由 `HealthManager.cpp` 内部的静态函数完成：

| Struct | JSON 序列化函数 | JSON 反序列化函数 | 对应表名 |
|--------|---------------|------------------|---------|
| VitalsRecord | `vitalsToJson()` | `jsonToVitals()` | `vitals_records` |
| LabTestRecord | `labTestToJson()` | `jsonToLabTest()` | `lab_test_records` |
| BloodPressureRecord | `bpToJson()` | `jsonToBp()` | `blood_pressure_records` |
| MedicalHistoryRecord | `medicalHistoryToJson()` | `jsonToMedicalHistory()` | `medical_history_records` |
| UserProfile | `userProfileToJson()` | `jsonToUserProfile()` | `user_profile` |

**转换规则**：
- C++ `camelCase` 成员 → JSON/DB `snake_case` key
- `std::optional<T>` 字段：仅含值时写入，缺失时跳过（DB 中为 NULL）
- `TimePoint` ↔ ISO 8601 字符串（`"2024-06-15T08:00:00Z"`）
- `bool` → `INTEGER`（0/1）

### 10.2 UserProfile 的 Upsert 语义

```cpp
saveUserProfile(profile):
    if (updateRecord("user_profile", id, json) succeeds with changes > 0):
        return true;    // 已更新
    else:
        return insertRecord("user_profile", json);  // 不存在，插入
```

### 10.3 AI 报告降级策略

```
generateAIReport(period):
    if llmService_ 未配置 → 降级为 generateHealthReport(period) [纯本地]
    if AI 调用失败 (网络/超时/异常) → 降级为 generateHealthReport(period)
    if AI 返回 error JSON → 降级为 generateHealthReport(period)
    成功 → 返回 AI 生成的 JSON/文本
```

### 10.4 追问上下文管理

- `generateAIReport()` 成功时自动缓存 `lastHealthContext_` + `lastPeriodLabel_`
- AI 失败时清除 `lastHealthContext_`，追问会返回"尚未生成报告"提示
- 追问不要求 JSON 格式输出，允许自然语言回复

---

## 11. 编译与构建

### 11.1 构建命令

```bash
# 配置（macOS，带测试）
cd build
cmake .. -DBUILD_TESTING=ON

# 编译
make -j$(nproc)

# 运行
./frontend/Health_Manager_App
```

### 11.2 构建目标

| 目标 | 类型 | 说明 |
|------|------|------|
| `health_backend` | STATIC 库 | 所有后端 .cpp 编译产物，供 frontend 链接 |
| `Health_Manager_App` | EXECUTABLE | Qt6 Widgets 桌面应用入口 |

### 11.3 编译器要求

- C++17 标准（`CMAKE_CXX_STANDARD 17`，`REQUIRED ON`）
- Clang/GCC：`-Wall -Wextra -Wpedantic`
- MSVC：`/W4 /permissive-`

---

## 12. 版本历史

| 日期 | 变更内容 |
|------|---------|
| 2026-06-11 | 初版：MVC 架构、数据模型、5 条 CRUD、ASCVD Calculator |
| 2026-06-19 | 全面更新：新增 MedicalHistoryRecord、LLMService 完整实现、MetabolicCalculator（TyG+CDRS）、TrendReport 新数据结构、generateAIReport/AI 追问、update/delete CRUD、PlatformCompat、前端 Qt6 Widgets 集成 |
