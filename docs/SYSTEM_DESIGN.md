# OmniHealth Profiler - 系统架构设计

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
│  │ HealthManager │  │  DataAccess   │  │  LLMService  │  │
│  │  (Controller) │  │  (SQLite3)   │  │(cpp-httplib) │  │
│  └───────────────┘  └──────────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### 分层职责

| 层级 | 模块 | 职责 | 技术栈 |
|------|------|------|--------|
| **View** | frontend/ | UI 渲染、用户输入捕获、菜单与窗口管理 | ImGui / GLFW / OpenGL |
| **Controller** | HealthManager | API 协调、输入验证、业务流程编排 | 纯 C++17 |
| **Model** | DataAccess | SQLite3 CRUD、数据迁移、加密解密 | SQLite3 / SQLCipher |
| **Service** | LLMService | HTTP 请求构建、Response 解析、Prompt 模板化 | cpp-httplib / nlohmann-json |

### 架构原则

1. **依赖方向**：View → Controller → Model/Service（单向依赖，底层不知上层）
2. **契约驱动**：View 与 Controller 之间仅通过 `HealthManager.h` 接口契约通信
3. **PIMPL 模式**：所有实现细节隐藏在 `.cpp` 文件中，通过工厂函数暴露

## 2. 核心类设计（初步）

### 2.1 数据模型继承体系

```
HealthRecord (基类: 抽象所有健康记录的共有字段)
├── VitalsRecord      (基础体征: 心率、步数、睡眠、体重)
├── LabTestRecord     (临床检验: 血糖、血脂、尿酸)
├── BloodPressureRecord (血压专项: 收缩压/舒张压)
└── MedicalHistoryRecord (病历摘要: 既往病史、手术史、过敏史)
```

#### HealthRecord (基类)

```cpp
struct HealthRecord {
    std::string id;                                    // UUID 唯一标识
    HealthRecordType recordType;                       // 枚举: VITALS / LAB / BP / HISTORY
    std::chrono::system_clock::time_point timestamp;   // 采集/录入时间
    std::optional<std::string> source;                 // 数据来源（手动/设备型号）
    std::optional<std::string> note;                   // 备注
};
```

#### VitalsRecord (派生)

```cpp
struct VitalsRecord : public HealthRecord {
    std::optional<double> heartRate;   // bpm
    std::optional<int> steps;          // 步数
    std::optional<double> sleepHours;  // 小时
    std::optional<double> weightKg;    // kg
    std::optional<double> heightCm;    // cm (用于 BMI 计算)
};
```

### 2.2 控制器

#### HealthManager (对外统一接口)

```cpp
class HealthManager {
public:
    // ---- CRUD ----
    virtual bool addVitalsRecord(const VitalsRecord& record) = 0;
    virtual bool addLabTestRecord(const LabTestRecord& record) = 0;
    virtual std::vector<VitalsRecord> getVitalsRecords(
        std::optional<std::chrono::system_clock::time_point> from,
        std::optional<std::chrono::system_clock::time_point> to) const = 0;

    // ---- 风险计算 ----
    virtual double calculateASCVDScore() const = 0;          // 10年动脉粥样硬化性心血管疾病风险
    virtual double calculateBMI() const = 0;                 // 身体质量指数
    virtual std::string getBMICategory() const = 0;          // BMI 分级描述

    // ---- 趋势分析 ----
    virtual TrendResult analyzeTrend(HealthMetricType type,
        std::chrono::system_clock::time_point from,
        std::chrono::system_clock::time_point to) const = 0;

    // ---- LLM 咨询 ----
    virtual std::string generateHealthReport() const = 0;
    virtual std::string askHealthAdvisor(const std::string& userQuery) const = 0;
};
```

### 2.3 数据访问层

```cpp
class DataAccess {
public:
    bool initialize(const std::string& dbPath);
    bool insertRecord(const std::string& table, const std::string& jsonValue);
    std::string queryRecords(const std::string& sql);
    bool executeMigration(int version);
};
```

### 2.4 LLM 服务层

```cpp
class LLMService {
public:
    bool configure(const std::string& endpoint, const std::string& apiKey, const std::string& model);
    std::string chat(const std::string& systemPrompt, const std::string& userMessage);
    std::string buildHealthContextPrompt(const std::vector<HealthRecord>& recentData);
};
```

## 3. 数据流设计

### 3.1 用户录入流程

```
User Input (Frontend ImGui Form)
    │
    ▼
HealthManager::addVitalsRecord(record)
    │
    ├── 输入验证 (值域检查)
    │
    ├── DataAccess::insertRecord()
    │       │
    │       └── SQLite INSERT
    │
    └── 返回 bool (成功/失败)
```

### 3.2 LLM 咨询流程

```
User Query (Frontend Chat Input)
    │
    ▼
HealthManager::askHealthAdvisor(query)
    │
    ├── 1. 从 DataAccess 拉取近期健康数据摘要
    │
    ├── 2. LLMService::buildHealthContextPrompt() 构建 System Prompt
    │       (注入: 用户年龄/性别/BMI/近期异常指标/用药史)
    │
    ├── 3. LLMService::chat() 发起 HTTPS POST 请求
    │       → POST https://api.openai.com/v1/chat/completions
    │       → Body: { model, messages: [{system}, {user}] }
    │
    ├── 4. 解析 Response JSON → 提取 assistant content
    │
    └── 5. 拼接免责声明后返回给 Frontend
```

## 4. 数据库设计

### 4.1 核心表结构 (SQLite)

```sql
-- 用户档案表
CREATE TABLE user_profile (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    birth_date TEXT,           -- ISO 8601
    gender TEXT,               -- MALE / FEMALE
    smoking_status TEXT,       -- NEVER / FORMER / CURRENT
    created_at TEXT DEFAULT (datetime('now'))
);

-- 基础体征记录表
CREATE TABLE vitals_records (
    id TEXT PRIMARY KEY,
    user_id TEXT,
    timestamp TEXT NOT NULL,
    heart_rate REAL,
    steps INTEGER,
    sleep_hours REAL,
    weight_kg REAL,
    height_cm REAL,
    source TEXT,
    note TEXT,
    FOREIGN KEY (user_id) REFERENCES user_profile(id)
);

-- 临床检验记录表
CREATE TABLE lab_test_records (
    id TEXT PRIMARY KEY,
    user_id TEXT,
    timestamp TEXT NOT NULL,
    fasting_glucose REAL,      -- mmol/L
    total_cholesterol REAL,    -- mmol/L
    ldl_c REAL,                -- mmol/L
    hdl_c REAL,                -- mmol/L
    triglycerides REAL,        -- mmol/L
    uric_acid REAL,            -- μmol/L
    note TEXT,
    FOREIGN KEY (user_id) REFERENCES user_profile(id)
);

-- 血压记录表
CREATE TABLE blood_pressure_records (
    id TEXT PRIMARY KEY,
    user_id TEXT,
    timestamp TEXT NOT NULL,
    systolic INTEGER,          -- mmHg
    diastolic INTEGER,         -- mmHg
    note TEXT,
    FOREIGN KEY (user_id) REFERENCES user_profile(id)
);
```

### 4.2 索引策略

```sql
CREATE INDEX idx_vitals_timestamp ON vitals_records(timestamp);
CREATE INDEX idx_lab_timestamp ON lab_test_records(timestamp);
CREATE INDEX idx_bp_timestamp ON blood_pressure_records(timestamp);
```

## 5. 项目目录结构（完整规划）

```
Health_Manager/
├── CMakeLists.txt                    # 根 CMake
├── .clinerules                       # AI 代码生成规约
├── docs/
│   ├── PRD.md                        # 产品需求文档
│   └── SYSTEM_DESIGN.md              # 本文件
├── backend/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── HealthManager.h           # 契约接口
│   │   ├── DataAccess.h              # 数据访问层接口
│   │   ├── LLMService.h              # LLM 服务接口
│   │   └── Models/
│   │       ├── HealthRecord.h        # 基类数据结构
│   │       ├── VitalsRecord.h        # 体征数据
│   │       ├── LabTestRecord.h       # 检验数据
│   │       └── BloodPressureRecord.h # 血压数据
│   └── src/
│       ├── HealthManager.cpp
│       ├── DataAccess.cpp
│       ├── LLMService.cpp
│       └── ASCVDCalculator.cpp       # 心血管风险计算器
├── frontend/
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp
│       ├── MainWindow.cpp            # 主窗口
│       ├── DashboardPanel.cpp        # 仪表盘
│       ├── DataEntryForm.cpp         # 数据录入表单
│       ├── TrendChart.cpp            # 趋势图组件
│       └── ChatPanel.cpp             # LLM 对话面板
└── third_party/                      # 备用外部库