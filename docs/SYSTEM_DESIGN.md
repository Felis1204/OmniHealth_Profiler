# OmniHealth Profiler - 系统架构设计

> 最后更新: 2026-06-11

## 1. 核心架构模式 (MVC)

```
┌─────────────────────────────────────────────────────────┐
│                    Frontend (View)                       │
│  main.cpp → UI 渲染 / 用户交互 / 事件分发                  │
│  依赖: ImGui (即将接入)                                   │
│  权限: 仅通过 HealthManager.h 契约调用后端                 │
└─────────────────────┬───────────────────────────────────┘
                      │  #include "HealthManager.h"
                      │  std::unique_ptr<HealthManager>
┌─────────────────────▼───────────────────────────────────┐
│                   Backend (Model & Logic)                │
│  ┌───────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ HealthManager │  │  DataAccess   │  │ASCVDCalc     │  │
│  │  (Controller) │  │  (SQLite3)   │  │(China-PAR)   │  │
│  └───────┬───────┘  └──────┬───────┘  └──────────────┘  │
│          │                 │                             │
│  ┌───────▼───────┐         │                             │
│  │   Models/     │         │                             │
│  │ HealthRecord  │         │                             │
│  │ VitalsRecord  │         │                             │
│  │ LabTestRecord │         │                             │
│  │ BPRecord      │         │                             │
│  │ UserProfile   │         │                             │
│  └───────────────┘         │                             │
│                            │                             │
│  ┌─────────────────────────▼──────────────────────┐      │
│  │              SQLite3 Database                   │      │
│  │  user_profile / vitals_records /                │      │
│  │  lab_test_records / blood_pressure_records      │      │
│  └────────────────────────────────────────────────┘      │
│                                                          │
│  ┌──────────────┐  (接口已定义，实现待完成)                 │
│  │  LLMService  │                                        │
│  │ (cpp-httplib)│                                        │
│  └──────────────┘                                        │
└─────────────────────────────────────────────────────────┘
```

## 2. 分层职责

| 层级 | 模块 | 职责 | 技术栈 | 状态 |
|------|------|------|--------|------|
| **View** | frontend/ | UI 渲染、用户输入捕获 | ImGui / GLFW | 📋 |
| **Controller** | HealthManager | API 协调、业务流程编排、算法调度 | C++17 | ✅ |
| **Model** | Models/ | 数据结构定义（5个struct + 基类） | C++17 | ✅ |
| **Persistence** | DataAccess | SQLite CRUD、建表、索引 | SQLite3 C API | ✅ |
| **Algorithm** | ASCVDCalculator | China-PAR 10年心血管风险评估 | C++17 `<cmath>` | ✅ |
| **Service** | LLMService | HTTP 请求构建、Prompt 模板化 | cpp-httplib | 📋 |

## 3. 架构原则

1. **依赖方向**：View → Controller → Model/Persistence/Algorithm（单向依赖）
2. **契约驱动**：View 与 Controller 之间仅通过 `HealthManager.h` 接口通信
3. **PIMPL 模式**：所有实现细节隐藏在 `.cpp` 文件中，通过工厂函数暴露
4. **纯计算分离**：算法模块（ASCVDCalculator）不依赖数据库，通过参数结构体传值，便于单元测试

## 4. 数据模型继承体系

```
HealthRecord (基类: id, recordType, timestamp, source, note)
├── VitalsRecord      (心率, 步数, 睡眠, 体重, 身高, 腰围)
├── LabTestRecord     (空腹血糖, 总胆固醇, LDL-C, HDL-C, 甘油三酯, 血尿酸)
├── BloodPressureRecord (收缩压, 舒张压)
└── MedicalHistoryRecord (待实现)

UserProfile (独立模型，非 HealthRecord 派生)
  ├── 基本信息: name, birthDate, gender
  ├── 风险因子: smokingStatus, hasDiabetes
  └── China-PAR 参数: region, urbanRural, familyHistoryASCVD
```

## 5. 核心类设计

### 5.1 HealthManager (对外统一接口)

```cpp
class HealthManager {
public:
    // ---- CRUD ----
    virtual bool addVitalsRecord(const VitalsRecord&) = 0;
    virtual bool addLabTestRecord(const LabTestRecord&) = 0;
    virtual bool addBloodPressureRecord(const BloodPressureRecord&) = 0;
    virtual bool saveUserProfile(const UserProfile&) = 0;

    virtual std::vector<VitalsRecord> getVitalsRecords(opt<TimePoint>, opt<TimePoint>) = 0;
    virtual std::vector<LabTestRecord> getLabTestRecords(opt<TimePoint>, opt<TimePoint>) = 0;
    virtual std::vector<BloodPressureRecord> getBloodPressureRecords(opt<TimePoint>, opt<TimePoint>) = 0;
    virtual std::optional<UserProfile> getUserProfile() = 0;

    // ---- 风险计算 ----
    virtual double calculateASCVDScore() const = 0;   // China-PAR
    virtual double calculateBMI() const = 0;
    virtual std::string getBMICategory() const = 0;

    // ---- 趋势分析 ----
    virtual TrendResult analyzeTrend(HealthRecordType, TimePoint, TimePoint) = 0;

    // ---- LLM 咨询 ----
    virtual std::string generateHealthReport() const = 0;
    virtual std::string askHealthAdvisor(const std::string&) const = 0;
};
```

### 5.2 DataAccess (数据访问层)

```cpp
class DataAccess {
public:
    virtual bool initialize(const std::string& dbPath) = 0;
    virtual bool insertRecord(const std::string& table, const std::string& jsonValue) = 0;
    virtual std::string queryRecords(const std::string& sql) = 0;
    virtual bool executeMigration(int version) = 0;
};
```
- 内部实现 PIMPL: `DataAccessImpl` 持有 `sqlite3* db_`
- 安全策略: 表名白名单 + 列名映射 + 预编译语句绑定
- 管理 5 张表 + 3 个时间戳索引

### 5.3 ASCVDCalculator (心血管风险算法)

```cpp
struct ASCVDParams {
    int age; double systolicBP; double fastingGlucose;
    double totalCholesterol; double hdlC; double waistCm;
    bool isMale; bool isCurrentSmoker; bool hasDiabetes;
    bool isNorthern; bool isUrban; bool hasFamilyHistory;
};

class ASCVDCalculator {
public:
    static double calculateChinaPAR(const ASCVDParams&);
    static std::string getRiskCategory(double riskPercentage);
};
```
- 纯计算类，无状态，不依赖 DB
- 男女双分支系数 + 交互项
- 五级分层: <5.0% 低危 / 5.0-7.4% 临界 / 7.5-9.9% 中危 / 10.0-19.9% 高危 / ≥20.0% 极高危

### 5.4 LLMService (大模型服务，接口已定义)

```cpp
class LLMService {
public:
    virtual bool configure(const std::string& endpoint, const std::string& apiKey, const std::string& model) = 0;
    virtual std::string chat(const std::string& systemPrompt, const std::string& userMessage) = 0;
    virtual std::string buildHealthContextPrompt(const std::vector<HealthRecord>&) = 0;
};
```

## 6. 数据库设计 (SQLite)

### 6.1 表结构

```sql
-- 用户档案表 (China-PAR 所需人口学参数)
CREATE TABLE user_profile (
    id TEXT PRIMARY KEY, name TEXT NOT NULL,
    birth_date TEXT, gender TEXT, smoking_status TEXT,
    region TEXT, urban_rural TEXT,
    family_history_ascvd INTEGER DEFAULT 0,
    has_diabetes INTEGER DEFAULT 0,
    created_at TEXT DEFAULT (datetime('now'))
);

-- 基础体征记录表 (含腰围用于代谢综合征/ASCVD)
CREATE TABLE vitals_records (
    id TEXT PRIMARY KEY, user_id TEXT, timestamp TEXT NOT NULL,
    heart_rate REAL, steps INTEGER, sleep_hours REAL,
    weight_kg REAL, height_cm REAL, waist_cm REAL,
    source TEXT, note TEXT,
    FOREIGN KEY (user_id) REFERENCES user_profile(id)
);

-- 临床检验记录表
CREATE TABLE lab_test_records (
    id TEXT PRIMARY KEY, user_id TEXT, timestamp TEXT NOT NULL,
    fasting_glucose REAL, total_cholesterol REAL,
    ldl_c REAL, hdl_c REAL, triglycerides REAL, uric_acid REAL,
    note TEXT,
    FOREIGN KEY (user_id) REFERENCES user_profile(id)
);

-- 血压记录表
CREATE TABLE blood_pressure_records (
    id TEXT PRIMARY KEY, user_id TEXT, timestamp TEXT NOT NULL,
    systolic INTEGER, diastolic INTEGER, note TEXT,
    FOREIGN KEY (user_id) REFERENCES user_profile(id)
);
```

### 6.2 索引

```sql
CREATE INDEX idx_vitals_timestamp ON vitals_records(timestamp);
CREATE INDEX idx_lab_timestamp ON lab_test_records(timestamp);
CREATE INDEX idx_bp_timestamp ON blood_pressure_records(timestamp);
```

### 6.3 数据库配置

- `PRAGMA journal_mode=WAL` — 提升并发读性能
- `PRAGMA foreign_keys=ON` — 启用外键约束

## 7. 数据流设计

### 7.1 用户录入流程

```
User Input (Frontend)
    │
    ▼
HealthManager::addVitalsRecord(record)
    │
    ├── vitalsToJson(record)     → JSON 序列化
    ├── DataAccess::insertRecord("vitals_records", json)
    │       ├── 表名白名单校验
    │       ├── 列名映射
    │       └── sqlite3_prepare_v2 + bind → INSERT
    └── 返回 bool
```

### 7.2 ASCVD 风险评估流程

```
HealthManager::calculateASCVDScore()
    │
    ├── getUserProfile()          → 年龄/性别/吸烟/糖尿病/地域/城乡/家族史
    ├── calculateAge(birthDate)   → 身份证年龄 (整岁)
    ├── getAverageSystolicBP(3)   → 近3次收缩压均值
    ├── getLabTestRecords()       → 最新 TC/HDL-C/空腹血糖
    ├── getVitalsRecords()        → 最新腰围
    │
    ├── 组装 ASCVDParams
    │
    └── ASCVDCalculator::calculateChinaPAR(params)
            ├── 年龄校验 (35-74)
            ├── TC/HDL 单位换算 (mmol/L ÷ 0.0259 → mg/dL)
            ├── 糖尿病自动判定 (Glu≥7.0 + 注明)
            ├── ln 变换
            ├── 男女分支公式 (含交互项)
            └── Risk = 1 - S10^exp(Sum - MeanXB)
```

## 8. 项目目录结构

```
Health_Manager/
├── CMakeLists.txt                    # 根 CMake (C++17)
├── .clinerules                       # AI 代码生成规约
├── .gitignore
├── docs/
│   ├── PRD.md                        # 产品需求文档
│   └── SYSTEM_DESIGN.md              # 本文件
├── backend/
│   ├── CMakeLists.txt                # 静态库: health_backend
│   ├── include/
│   │   ├── HealthManager.h           # 控制器接口 (15 个纯虚方法)
│   │   ├── DataAccess.h              # 数据访问层接口
│   │   ├── LLMService.h              # LLM 服务接口 (待实现)
│   │   ├── ASCVDCalculator.h         # China-PAR 算法 (ASCVDParams + 计算器)
│   │   └── Models/
│   │       ├── HealthRecord.h        # 基类 + TimePoint + HealthRecordType
│   │       ├── VitalsRecord.h        # 体征 (含腰围)
│   │       ├── LabTestRecord.h       # 临床检验
│   │       ├── BloodPressureRecord.h # 血压
│   │       └── UserProfile.h         # 用户档案
│   └── src/
│       ├── HealthManager.cpp         # HealthManagerImpl (~550行)
│       ├── DataAccess.cpp            # DataAccessImpl (~390行)
│       └── ASCVDCalculator.cpp       # China-PAR 实现 (~170行)
├── frontend/
│   ├── CMakeLists.txt
│   └── src/
│       └── main.cpp                  # 命令行测试入口
└── build/                            # 构建输出 (gitignored)
```

## 9. 依赖管理

| 依赖 | 版本 | 集成方式 | 用途 |
|------|------|---------|------|
| SQLite3 | 3.53.2 | Homebrew + find_package | 本地数据持久化 |
| nlohmann/json | 3.11.3 | FetchContent (GitHub) | JSON 序列化/反序列化 |
| cpp-httplib | 📋 | FetchContent | LLM API HTTP 请求 |
