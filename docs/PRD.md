# OmniHealth Profiler（全景健康数字孪生系统）- 产品需求文档

> 最后更新: 2026-06-18 | 开发进度: Phase 2 完成

---

## 1. 产品定位

OmniHealth Profiler 是一款基于 C++ 桌面端的**个人全生命周期健康数据管理与智能分析引擎**。

**核心价值主张**：让用户掌握自己的完整健康画像，通过本地私有化数据存储保障隐私，同时借助大模型能力（RAG 健康上下文注入）获得专业级健康解读与个性化咨询。

**差异化优势**：
- **隐私优先**：全部健康数据存储在本地 SQLite 文件中，不经过任何云端中转。LLM 调用时对个人信息（姓名、ID）做脱敏处理，仅发送数值型健康指标。
- **医学循证**：内置 China-PAR、TyG、CDRS 等基于中国人群的大样本人群队列研究算法，非通用公式照搬。
- **AI 赋能但本地兜底**：AI 报告与追问基于 DeepSeek API，网络不可用时自动降级为本地统计分析报告，保证系统永远可用。
- **跨平台**：同时支持 macOS（Apple Silicon / Intel）与 Windows（MinGW / MSVC）。

---

## 2. 目标用户

| 用户群体 | 核心诉求 | 对应功能 |
|---------|---------|---------|
| 关注自身健康的个人用户 | 长期追踪体征变化，了解健康趋势 | Vitals/BP 录入 + 趋势分析 |
| 有慢性病管理需求的患者（高血压、糖尿病、高脂血症） | 定期录入检验数据，获得风险评估与用药建议 | LabTest 录入 + TyG/CDRS + AI 报告 |
| 心血管中高危人群（35 岁以上） | 评估 10 年 ASCVD 风险，指导一级预防 | ASCVD China-PAR 五级分层 |
| 亚健康人群（体检异常但无明显症状） | 追踪指标变化，提前预警 | 趋势分析 + AI 追问 |

---

## 3. 核心功能需求

### 3.1 多源数据融合（✅ 已实现）

支持录入 5 种健康数据类型，统一存储于本地 SQLite 数据库。所有数据类型均支持完整的增删改查操作。

| 数据类别 | 包含指标 | 数据模型 | CRUD | 说明 |
|---------|---------|---------|------|------|
| 基础体征 | 心率、步数、睡眠时长、体重、身高、腰围 | VitalsRecord | ✅ | 6 个量化字段 |
| 临床检验 | 空腹血糖、总胆固醇、LDL-C、HDL-C、甘油三酯、血尿酸 | LabTestRecord | ✅ | 6 个量化字段 |
| 血压专项 | 收缩压、舒张压 | BloodPressureRecord | ✅ | 2 个量化字段 |
| 用户档案 | 姓名、出生日期、性别、吸烟、地域、城乡、家族史 ASCVD、糖尿病 | UserProfile | ✅ | upsert 模式（save） |
| 病历摘要 | 既往病史、手术史、过敏史、家族史、用药史、其他 | MedicalHistoryRecord | ✅ | category + content 自由文本 |

**查询能力**：
- Vitals / LabTest / BloodPressure：按时间范围查询（支持无上限/无下限）
- MedicalHistory：获取全部记录，按时间降序
- UserProfile：单用户系统，取第一条记录

### 3.2 纵深算法评价（✅ 核心算法全部实现）

根据医学指南建立量化评价体系，提供长期的健康趋势分析。

#### 3.2.1 心血管与代谢风险评估

| 算法 | 循证依据 | 功能 | 状态 |
|------|---------|------|------|
| **ASCVD China-PAR** | Yang X et al. *Circulation* 2016;134:1430-1440 | 10 年动脉粥样硬化性心血管疾病风险评分 | ✅ |
| **ASCVD 五级分层** | 同一研究 | 低危（<5.0%）/ 临界（5.0-7.4%）/ 中危（7.5-9.9%）/ 高危（10.0-19.9%）/ 极高危（≥20.0%） | ✅ |
| **BMI 计算 + 分级** | 中国成人体重标准 | 偏瘦 / 正常 / 超重 / 肥胖（四级分类） | ✅ |
| **TyG 指数** | Dicky et al. *Diabetes & Metabolic Syndrome: CRR* 2022 | 胰岛素抵抗筛查：TyG ≥ 8.70 提示存在胰岛素抵抗 | ✅ |
| **CDRS** | Gao et al. *Diabetic Medicine* 2010 | 中国糖尿病风险评分，按性别独立切点（男≥17 / 女≥14 高风险） | ✅ |

**算法架构特点**：纯计算类（无状态，不依赖数据库），通过参数结构体传值，便于单元测试和前端独立调用。

#### 3.2.2 趋势分析（两套接口）

| 接口 | 功能 | 状态 |
|------|------|------|
| `analyzeTrend(type, from, to)` | 旧版：单个默认指标的统计摘要（均值/最小/最大/中位数/线性回归斜率） | ✅ |
| `analyzeTrendReport(type, from, to)` | 新版：所有可量化指标的时间-数值数据点序列 + 统计摘要 + 趋势方向 | ✅ |

`analyzeTrendReport` 返回 `TrendReport` 结构，每个可量化指标包含以下字段：`metricName`（中文名）、`metricKey`（键）、`unit`（单位）、`dataPoints`（时间序列点）、`average` / `min` / `max` / `median` / `slope` / `count`。前端可直接用于折线图渲染。

#### 3.2.3 健康报告生成（三套接口）

| 接口 | 功能 | 状态 |
|------|------|------|
| `generateHealthReport()` | 快照报告：最新单点数据 + BMI + ASCVD + 医学解读 | ✅ |
| `generateHealthReport(WEEKLY/MONTHLY)` | 周期报告：时段内各指标均值 + BMI + ASCVD + 趋势概览 | ✅ |
| `generateAIReport(WEEKLY/MONTHLY)` | AI 增强报告：DeepSeek API 生成 JSON 格式个性化报告 | ✅ |
| `askFollowUp(question)` | AI 追问：基于最近一次报告的健康数据上下文，进行连续对话 | ✅ |
| `getStatistics(type)` | 统计摘要：min / max / avg + 记录条数 | ✅ |

**AI 报告流程（AI-First 策略）**：
1. 提取时段内的体征/血压/检验数据 + 用户档案 + 病历摘要
2. 组装脱敏后的 User Prompt（含异常标记 + 风险计算结果）
3. 调用 LLMService → DeepSeek API
4. 若调用成功 → 返回 JSON（含 keys_cn / risk_assessment / lifestyle / follow_up / medications / red_flags）
5. 若调用失败（网络/超时/异常）→ 自动降级为本地 `generateHealthReport(period)`
6. 若 LLM 未配置 API Key → 直接返回本地报告

**脱敏策略**：姓名 / ID 不会发送给 API。仅发送年龄（由出生日期计算）、性别、地区类型、城市/乡村等人口学标签。

#### 3.2.4 病历摘要 AI 集成

病历摘要记录在生成 AI 报告时会注入 User Prompt，使 AI 能够结合用户的既往病史（如 "2 型糖尿病 5 年"、"青霉素过敏"）给出个性化建议。

#### 3.2.5 追问安全约束

`askFollowUp` 的 System Prompt 包含年龄/疾病/过敏安全约束：
- 禁止给未成年人推荐非处方药物
- 禁止给糖尿病患者推荐高糖饮食建议
- 禁止给已知过敏体质用户推荐相关药物
- 随访建议必须明确标注"请咨询医生后再执行"

---

### 3.3 大模型私有顾问（✅ 已完整实现）

#### 3.3.1 LLMService 接口

| 方法 | 功能 | 状态 |
|------|------|------|
| `configure(endpoint, apiKey, model)` | 配置 API 连接（支持从环境变量 `OPENAI_API_KEY` 读取） | ✅ |
| `isConfigured()` | 检查是否已配置 API Key | ✅ |
| `chat(systemPrompt, userMessage)` | 发起对话请求（OpenAI 兼容 API 格式） | ✅ |
| `buildHealthContextPrompt(...)` | 组装脱敏后的健康上下文 User Prompt（含异常标记） | ✅ |
| `buildSystemPrompt(periodLabel)` | 组装 System Prompt（医疗专家人设 + 强制 JSON 输出约束 + 结尾追问引导） | ✅ |
| `buildFollowUpSystemPrompt(healthContext)` | 组装追问 System Prompt（安全约束 + 病历上下文） | ✅ |
| `buildFollowUpUserPrompt(userQuestion)` | 组装追问 User Prompt | ✅ |

#### 3.3.2 技术细节

- **通信层**：cpp-httplib 纯头文件库，发起 HTTPS POST 请求
- **API 格式**：OpenAI 兼容 Chat Completions 端点
- **默认模型**：DeepSeek (`deepseek-v4-pro`)
- **超时**：60 秒
- **错误处理**：自动捕获异常并降级

---

## 4. 技术选型

| 技术 | 版本 | 集成方式 | 用途 | 状态 |
|------|------|---------|------|------|
| C++17 | - | 编译器原生 | 核心语言标准 | ✅ |
| CMake | ≥3.20 | 系统安装 | 跨平台构建系统 | ✅ |
| SQLite3 | 3.53.2 | Homebrew `find_package` | 本地数据持久化（WAL 模式 + 外键约束） | ✅ |
| nlohmann/json | 3.11.3 | FetchContent（GitHub） | JSON 序列化/反序列化 + AI 报告解析 | ✅ |
| cpp-httplib | latest | FetchContent（GitHub） | LLM API HTTPS 请求 | ✅ |
| Qt6 | 6.x | Homebrew `find_package` | 前端 UI 框架（Widgets） | ✅ |
| DeepSeek API | - | 云端 SaaS | AI 健康报告生成 + 追问 | ✅ |

### 4.1 选型依据

- **C++17**：高性能、跨平台，适合桌面端计算密集型场景。`std::optional`、`std::string_view`、结构化绑定等特性简化代码。
- **SQLite3**：嵌入式零配置数据库，数据以单文件形式存储，用户可随时备份/迁移。WAL 模式提升并发读性能。
- **nlohmann/json**：纯头文件、语法简洁的 JSON 库，广泛用于 C++ 生态。
- **cpp-httplib**：纯头文件 HTTP/HTTPS 客户端库，零外部依赖（仅需系统 OpenSSL），适合 LLM API 调用场景。
- **Qt6 Widgets**：成熟的 C++ 桌面 UI 框架，提供丰富的控件和信号/槽机制，适合数据录入表单和图表展示。
- **CMake + FetchContent**：现代化构建系统，依赖管理简洁可靠，无需手动管理第三方库版本。

---

## 5. 模块完成度

### 5.1 数据模型

| 模块 | 文件 | 字段数 | 完成度 | 
|------|------|--------|--------|
| HealthRecord（基类） | `Models/HealthRecord.h` | 5（id, type, timestamp, source, note） | ✅ 100% |
| VitalsRecord | `Models/VitalsRecord.h` | 6（heartRate, steps, sleepHours, weightKg, heightCm, waistCm） | ✅ 100% |
| LabTestRecord | `Models/LabTestRecord.h` | 6（fastingGlucose, TC, LDL-C, HDL-C, TG, uricAcid） | ✅ 100% |
| BloodPressureRecord | `Models/BloodPressureRecord.h` | 2（systolic, diastolic） | ✅ 100% |
| MedicalHistoryRecord | `Models/MedicalHistoryRecord.h` | 2（category, content） | ✅ 100% |
| UserProfile | `Models/UserProfile.h` | 8（name, birthDate, gender, smoking, region, urbanRural, familyASCVD, hasDiabetes） | ✅ 100% |

### 5.2 数据库持久化（DataAccess）

| 功能 | 完成度 |
|------|--------|
| 数据库初始化（建表 + 索引 + WAL + 外键） | ✅ 100% |
| vitals_records 表（CREATE + INSERT + UPDATE + DELETE + SELECT by time） | ✅ 100% |
| lab_test_records 表（CREATE + INSERT + UPDATE + DELETE + SELECT by time） | ✅ 100% |
| blood_pressure_records 表（CREATE + INSERT + UPDATE + DELETE + SELECT by time） | ✅ 100% |
| medical_history_records 表（CREATE + INSERT + UPDATE + DELETE + SELECT all） | ✅ 100% |
| user_profile 表（CREATE + INSERT/UPDATE(upsert) + DELETE + SELECT） | ✅ 100% |
| 时间戳索引（4 个） | ✅ 100% |
| 预编译语句 + 参数绑定（防 SQL 注入） | ✅ 100% |
| 表名白名单 + 列名映射安全校验 | ✅ 100% |

### 5.3 业务逻辑（HealthManager）

| 功能 | 接口 | 完成度 |
|------|------|--------|
| Vitals CRUD | add / update / delete / get(by time range) | ✅ 100% |
| LabTest CRUD | add / update / delete / get(by time range) | ✅ 100% |
| BloodPressure CRUD | add / update / delete / get(by time range) | ✅ 100% |
| MedicalHistory CRUD | add / update / delete / get(all, by time desc) | ✅ 100% |
| UserProfile CRUD | save(upsert) / get / delete | ✅ 100% |
| ASCVD China-PAR | calculateASCVDScore() | ✅ 100% |
| BMI + 分级 | calculateBMI() + getBMICategory() | ✅ 100% |
| TyG 指数 | calculateTyGIndex() | ✅ 100% |
| CDRS | calculateCDRS() | ✅ 100% |
| 旧版趋势分析 | analyzeTrend() | ✅ 100% |
| 新版趋势报告 | analyzeTrendReport() | ✅ 100% |
| 快照健康报告 | generateHealthReport() | ✅ 100% |
| 周期健康报告 | generateHealthReport(WEEKLY/MONTHLY) | ✅ 100% |
| AI 增强报告 | generateAIReport(WEEKLY/MONTHLY) | ✅ 100% |
| AI 追问 | askFollowUp(question) | ✅ 100% |
| 统计摘要 | getStatistics(type) | ✅ 100% |

### 5.4 算法模块

| 模块 | 文件 | 算法 | 完成度 |
|------|------|------|--------|
| ASCVDCalculator | `ASCVDCalculator.h/.cpp` | China-PAR 男女双分支系数 + 交互项 + ln 变换 + 生存率公式 | ✅ 100% |
| MetabolicCalculator | `MetabolicCalculator.h/.cpp` | TyG = ln(TG×FPG/2) + CDRS 累加评分 | ✅ 100% |
| 趋势分析（内置） | `HealthManager.cpp` | 均值/最小/最大/中位数/线性回归斜率 | ✅ 100% |

### 5.5 AI / LLM 服务

| 模块 | 完成度 |
|------|--------|
| LLMService 接口定义 | ✅ 100% |
| LLMService 实现（cpp-httplib + DeepSeek API） | ✅ 100% |
| System Prompt 组装（医疗专家人设 + JSON 格式约束 + 追问引导） | ✅ 100% |
| User Prompt 组装（脱敏 + 异常标记 + 病历注入） | ✅ 100% |
| Follow-Up System Prompt（年龄/疾病/过敏安全约束） | ✅ 100% |
| RAG 健康上下文注入 | ✅ 100% |
| 自动降级策略（AI 失败 → 本地报告） | ✅ 100% |
| 环境变量 API Key 支持（`OPENAI_API_KEY`） | ✅ 100% |

### 5.6 前端 UI

| 功能 | 完成度 | 说明 |
|------|--------|------|
| Qt6 Widgets 集成 | ✅ 100% | 替代早期规划的 ImGui |
| 数据录入表单 | 📋 进行中 | 5 种记录类型的录入界面 |
| 趋势折线图渲染 | 📋 进行中 | 基于 analyzeTrendReport 返回的数据 |
| 风险评估展示面板 | 📋 进行中 | ASCVD / BMI / TyG / CDRS 结果展示 |
| AI 报告展示 + 追问交互 | 📋 进行中 | 基于 generateAIReport + askFollowUp |

### 5.7 跨平台兼容

| 平台 | 编译器 | 状态 |
|------|------|------|
| macOS（Apple Silicon） | Apple Clang | ✅ 已验证 |
| macOS（Intel） | Apple Clang | ✅ 自动化检测 Homebrew 路径 |
| Windows | MinGW / MSVC | ✅ 条件编译 `gmtimeCompat` / `timegmCompat` |

---

## 6. 数据库设计

### 6.1 表结构

```sql
-- 用户档案表（China-PAR 所需人口学参数）
CREATE TABLE user_profile (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    birth_date TEXT,
    gender TEXT,
    smoking_status TEXT,
    region TEXT,
    urban_rural TEXT,
    family_history_ascvd INTEGER DEFAULT 0,
    has_diabetes INTEGER DEFAULT 0,
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
    waist_cm REAL,
    source TEXT,
    note TEXT,
    FOREIGN KEY (user_id) REFERENCES user_profile(id)
);

-- 临床检验记录表
CREATE TABLE lab_test_records (
    id TEXT PRIMARY KEY,
    user_id TEXT,
    timestamp TEXT NOT NULL,
    fasting_glucose REAL,
    total_cholesterol REAL,
    ldl_c REAL,
    hdl_c REAL,
    triglycerides REAL,
    uric_acid REAL,
    note TEXT,
    FOREIGN KEY (user_id) REFERENCES user_profile(id)
);

-- 血压记录表
CREATE TABLE blood_pressure_records (
    id TEXT PRIMARY KEY,
    user_id TEXT,
    timestamp TEXT NOT NULL,
    systolic INTEGER,
    diastolic INTEGER,
    note TEXT,
    FOREIGN KEY (user_id) REFERENCES user_profile(id)
);

-- 病历摘要记录表
CREATE TABLE medical_history_records (
    id TEXT PRIMARY KEY,
    user_id TEXT,
    timestamp TEXT NOT NULL,
    category TEXT,
    content TEXT,
    source TEXT,
    note TEXT,
    FOREIGN KEY (user_id) REFERENCES user_profile(id)
);
```

### 6.2 索引

```sql
CREATE INDEX idx_vitals_timestamp    ON vitals_records(timestamp);
CREATE INDEX idx_lab_timestamp       ON lab_test_records(timestamp);
CREATE INDEX idx_bp_timestamp        ON blood_pressure_records(timestamp);
CREATE INDEX idx_mh_timestamp        ON medical_history_records(timestamp);
```

### 6.3 数据库配置

- `PRAGMA journal_mode=WAL` — 提升并发读性能
- `PRAGMA foreign_keys=ON` — 启用外键约束

---

## 7. 项目目录结构

```
Health_Manager/
├── CMakeLists.txt                      # 根 CMake（C++17）
├── CLAUDE.md                           # AI 代码生成规约
├── .gitignore
├── docs/
│   ├── PRD.md                          # 本文件
│   └── SYSTEM_DESIGN.md                # 系统架构设计
├── backend/
│   ├── CMakeLists.txt                  # 静态库: health_backend
│   ├── include/
│   │   ├── HealthManager.h             # 控制器接口（25+ 纯虚方法）
│   │   ├── DataAccess.h                # 数据访问层接口
│   │   ├── LLMService.h                # LLM 服务接口（含 Prompt 组装）
│   │   ├── ASCVDCalculator.h           # China-PAR 算法
│   │   ├── MetabolicCalculator.h       # TyG + CDRS 算法
│   │   ├── PlatformCompat.h            # 跨平台兼容（gmtime/timegm）
│   │   └── Models/
│   │       ├── HealthRecord.h          # 基类 + TimePoint + HealthRecordType
│   │       ├── VitalsRecord.h          # 体征（6 字段）
│   │       ├── LabTestRecord.h         # 临床检验（6 字段）
│   │       ├── BloodPressureRecord.h    # 血压（2 字段）
│   │       ├── MedicalHistoryRecord.h   # 病历摘要（category + content）
│   │       └── UserProfile.h           # 用户档案（8 字段）
│   └── src/
│       ├── HealthManager.cpp           # HealthManagerImpl
│       ├── DataAccess.cpp              # DataAccessImpl（SQLite3）
│       ├── LLMService.cpp             # LLMServiceImpl（cpp-httplib）
│       ├── ASCVDCalculator.cpp         # China-PAR 实现
│       └── MetabolicCalculator.cpp     # TyG + CDRS 实现
├── frontend/
│   ├── CMakeLists.txt
│   └── src/
│       └── main.cpp                    # Qt6 前端入口
└── build/                              # 构建输出（gitignored）
```

---

## 8. 待开发功能（Backlog）

| 优先级 | 功能 | 类别 | 说明 |
|--------|------|------|------|
| P1 | 前端数据录入表单 | UI | 5 种记录类型（Vitals / LabTest / BP / MedicalHistory / UserProfile）的 Qt6 录入界面 |
| P1 | 前端趋势折线图 | UI | 基于 `analyzeTrendReport` 返回的 MetricTrend 数据渲染多指标折线图 |
| P1 | 前端风险评估面板 | UI | ASCVD 五级仪表盘 + BMI 分级 + TyG/CDRS 结果卡片 |
| P1 | 前端 AI 报告展示 + 追问 | UI | `generateAIReport` 结果解析展示 + `askFollowUp` 对话交互 |
| P2 | 代谢综合征诊断 | 算法 | ATP III 标准：腰围 + TG + HDL-C + BP + FPG 五项中满足三项即诊断 |
| P2 | 血压分级 | 算法 | ACC/AHA 2017 标准：正常 / 升高 / 高血压1期 / 高血压2期 / 高血压危象 |
| P2 | eGFR 肾功能评估 | 算法 | CKD-EPI 2021 公式：基于肌酐 + 年龄 + 性别估算肾小球滤过率 |
| P3 | 数据导入/导出 | 功能 | CSV / JSON 格式导入导出，方便从其他健康应用迁移数据 |
| P3 | 多用户支持 | 功能 | 当前为单用户系统，未来支持家庭成员多档案 |
| P3 | 定期提醒 | 功能 | 系统托盘提醒用户定期录入血压/血糖/体重等指标 |
| P3 | 数据可视化增强 | UI | 多指标同图对比、异常值高亮、趋势预测线 |

---

## 9. 非功能需求

### 9.1 性能

- 数据库查询（单表按时间范围）：< 100ms（万条级别记录）
- AI 报告生成：< 60s（含 HTTPS 请求往返）
- 所有风险计算（ASCVD / BMI / TyG / CDRS）：< 1ms（纯 CPU 浮点运算）

### 9.2 安全与隐私

- **本地存储**：所有健康数据存储在本地 SQLite 文件，不上传任何云端
- **脱敏**：LLM API 调用时移除姓名、ID 等个人标识信息
- **加密**：未来版本支持 SQLite 加密扩展（SQLCipher）
- **无追踪**：不收集任何用户行为数据，不接入任何分析 SDK

### 9.3 可靠性

- AI 服务不可用时自动降级为本地报告生成
- 数据库操作使用预编译语句，防止 SQL 注入
- 所有 CRUD 操作返回 bool，调用方需检查返回值

### 9.4 可维护性

- 严格遵循 MVC 分层（View / Controller / Model / Persistence / Algorithm）
- PIMPL 模式隐藏实现细节
- Doxygen 风格注释覆盖所有公开接口
- 纯算法类无状态、不依赖数据库，便于单元测试

---

## 10. 版本历史

| 版本 | 日期 | 变更内容 |
|------|------|---------|
| 0.1.0 | 2026-06 初 | Phase 1 完成：数据模型 + CRUD + ASCVD + BMI + 旧版趋势分析 |
| 0.2.0 | 2026-06-18 | Phase 2 完成：MedicalHistory CRUD + TyG + CDRS + analyzeTrendReport + AI 报告 + 追问 + LLMService 完整实现 + 降级策略 + Qt6 迁移 |
