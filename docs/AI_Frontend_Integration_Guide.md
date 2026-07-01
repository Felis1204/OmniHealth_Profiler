# AI 功能前端集成与操作指南

> 适用于 OmniHealth Profiler 前端开发人员，说明如何使用新增的 AI 配置与报告接口。

---

## 1. 架构概览

```
┌──────────────────────────────────────────────────────┐
│  前端 (Qt6 Widgets)                                   │
│                                                       │
│  ┌──────────────────┐   ┌──────────────────────────┐ │
│  │ AISettingsDialog  │   │  AIReportDialog           │ │
│  │                   │   │                          │ │
│  │  Endpoint ┌────┐  │   │  [生成报告] 周报/月报     │ │
│  │  API Key  │****│  │   │  ┌──────────────────┐   │ │
│  │  Model    └────┘  │   │  │  AI 报告 / 追问   │   │ │
│  │  [保存] [测试]     │   │  │  对话展示区       │   │ │
│  └────────┬─────────┘   │  └──────────────────┘   │ │
│           │              │  [输入追问...] [发送]    │ │
│           ▼              └──────────┬───────────────┘ │
│  ┌──────────────────────────────────┴──────────────┐  │
│  │         HealthManager (统一接口)                  │  │
│  │  configureLLM() / isLLMConfigured()              │  │
│  │  generateAIReport() / askFollowUp()              │  │
│  └──────────────────────┬───────────────────────────┘  │
└─────────────────────────┼──────────────────────────────┘
                          ▼
              ┌───────────────────────┐
              │     后端               │
              │  LLMService            │
              │  → DeepSeek API        │
              │  → 降级: 本地报告       │
              └───────────────────────┘
```

---

## 2. 新增后端接口

### 2.1 `configureLLM()` — 配置 AI 连接

```cpp
bool configureLLM(const std::string& endpoint,
                  const std::string& apiKey = "",
                  const std::string& model = "deepseek-v4-pro");
```

**参数说明**：
| 参数 | 必填 | 默认值 | 说明 |
|------|------|--------|------|
| `endpoint` | ✅ | 无 | OpenAI 兼容 API URL |
| `apiKey` | ❌ | `""` | 为空则读 `OPENAI_API_KEY` 环境变量 |
| `model` | ❌ | `"deepseek-v4-pro"` | 模型名 |

**常用 endpoint**：
| 服务商 | URL |
|--------|-----|
| DeepSeek | `https://api.deepseek.com/chat/completions` |
| OpenAI | `https://api.openai.com/v1/chat/completions` |
| Ollama (本地) | `http://localhost:11434/v1/chat/completions` |

### 2.2 `isLLMConfigured()` — 检查配置

```cpp
bool isLLMConfigured() const;
```

返回 `true` 表示 API Key 已设置，可以正常使用 AI 功能。

---

## 3. 前端对话框使用说明

### 3.1 AI 设置对话框 (`AISettingsDialog`)

**文件**：`frontend/include/AISettingsDialog.h` / `frontend/src/AISettingsDialog.cpp`

**功能**：让用户输入 Endpoint、API Key、Model 三项参数，保存到后端。

```cpp
#include "AISettingsDialog.h"

// 打开设置对话框
void MainWindow::openAISettings() {
    AISettingsDialog dlg(manager_.get(), this);
    if (dlg.exec() == QDialog::Accepted) {
        // 配置保存成功，AI 功能可用
        updateAIButtonStates();
    }
}
```

**UI 组件**：
- **Endpoint** — 文本输入框，预填 `https://api.deepseek.com/chat/completions`
- **API Key** — 密码遮罩输入框（`EchoMode::Password`）
- **Model** — 文本输入框，预填 `deepseek-v4-pro`
- **状态指示**：
  - ✅ 绿色 — 已配置
  - ⚠️ 橙色 — 未配置
- **测试连接**按钮 — 立即用当前配置尝试生成一份周报
- **保存**按钮 — 调用 `manager_->configureLLM()` 持久化

### 3.2 AI 报告对话框 (`AIReportDialog`)

**文件**：`frontend/include/AIReportDialog.h` / `frontend/src/AIReportDialog.cpp`

**功能**：生成 AI 报告 + 追问交互。

```cpp
#include "AIReportDialog.h"

// 打开 AI 报告对话框
void MainWindow::openAIReport() {
    AIReportDialog dlg(manager_.get(), this);
    dlg.exec();
}
```

**操作流程**：
1. 选择报告周期（周报/月报）
2. 点击「生成报告」
3. AI 返回 JSON 报告 → 显示在对话区
4. 在底部输入框输入追问 → 点击「发送」
5. 可多次追问（上下文由后端缓存）

**交互状态**：
| 状态 | 追问输入框 | 说明 |
|------|-----------|------|
| 初始 | 禁用 | 提示"请先生成 AI 报告" |
| AI 报告生成成功 | 启用 | 可无限追问 |
| AI 失败/降级 | 禁用 | 降级报告不支持追问 |

---

## 4. 主窗口集成清单

`frontend/src/widget.cpp` 已实现以下按钮：

| 按钮 | 所属 GroupBox | 触发的槽函数 | 功能 |
|------|-------------|-------------|------|
| 🤖 AI 健康报告 | AI 健康顾问 | `on_aiReportButton_clicked()` | 打开 AIReportDialog |
| ⚙️ AI 设置 | AI 健康顾问 | `on_aiSettingsButton_clicked()` | 打开 AISettingsDialog |
| 📋 本地快照报告 | AI 健康顾问 | `on_localReportButton_clicked()` | 在 textBrowser 中显示本地快照 |
| 📊 风险总览 | 风险评估 | `on_riskButton_clicked()` | 显示 BMI/ASCVD/TyG/CDRS 评估结果 |

**启动检查**：构造函数中调用 `isLLMConfigured()` 更新按钮 tooltip。

---

## 5. 数据流示意

```
用户点击「AI 健康报告」
    │
    ▼
AIReportDialog::onGenerateClicked()
    │
    ├── 检查 manager_->isLLMConfigured()
    │   └── false → 直接调用 generateHealthReport(period) 降级
    │
    └── true → manager_->generateAIReport(period)
                │
                ├── 成功 → JSON → 显示 AI 报告，启用追问
                └── 失败 → 降级文本 → 显示本地报告，禁用追问

用户在 AIReportDialog 追问
    │
    ▼
AIReportDialog::onSendFollowUpClicked()
    │
    └── manager_->askFollowUp(question)
        │
        ├── 有上下文 → AI 回复
        └── 无上下文 → 返回提示"请先生成报告"
```

---

## 6. 错误处理对照表

| 场景 | 后端行为 | 前端处理 |
|------|---------|---------|
| 未配置 API Key | `generateAIReport()` 降级为本地报告 | `isLLMConfigured()` 返回 false，按钮 tooltip 提示 |
| API 网络超时 | 降级为本地报告，返回带 "⚠️ AI 服务暂不可用" 前缀的文本 | 检测到降级标记，禁用追问 |
| API 返回错误 JSON | 同上 | 同上 |
| 未生成报告直接追问 | `askFollowUp()` 返回 "尚未生成健康报告" | 提示用户先生成报告 |
| API Key 无效 (401) | `generateAIReport()` 降级 | 测试连接时提示用户检查 |

---

## 7. 文件清单

### 新增文件
| 文件 | 说明 |
|------|------|
| `frontend/include/AISettingsDialog.h` | AI 设置对话框头文件 |
| `frontend/src/AISettingsDialog.cpp` | AI 设置对话框实现 |
| `frontend/include/AIReportDialog.h` | AI 报告对话框头文件 |
| `frontend/src/AIReportDialog.cpp` | AI 报告对话框实现 |

### 修改文件
| 文件 | 变更内容 |
|------|---------|
| `backend/include/HealthManager.h` | 新增 `configureLLM()` + `isLLMConfigured()` |
| `backend/src/HealthManager.cpp` | 实现上述两个方法 |
| `frontend/Forms/widget.ui` | 新增 AI 健康顾问、风险评估 GroupBox |
| `frontend/include/widget.h` | 新增 AI 相关槽函数声明 |
| `frontend/src/widget.cpp` | 实现 AI 相关槽函数 |
| `frontend/CMakeLists.txt` | 新增 AISettingsDialog + AIReportDialog 源文件 |
| `docs/HealthManager_Frontend_API_Reference.md` | 更新 LLM 配置接口文档 |

---

## 8. 编译与运行

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/frontend/Health_Manager_App
```

---

> 📅 文档更新日期: 2026-07-01
