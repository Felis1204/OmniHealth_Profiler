#include "AISettingsDialog.h"
#include "HealthManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>

AISettingsDialog::AISettingsDialog(health::HealthManager* mgr,
                                   QWidget* parent)
    : QDialog(parent)
    , manager_(mgr)
{
    buildUI();
    updateStatusLabel();
}

// ============================================================
// 构建 UI（纯代码，无需 .ui 文件）
// ============================================================
void AISettingsDialog::buildUI()
{
    setWindowTitle("AI 顾问配置");
    setMinimumSize(480, 320);
    setMaximumSize(600, 400);

    auto* mainLayout = new QVBoxLayout(this);

    // ---- 说明文字 ----
    auto* hintLabel = new QLabel(
        "配置大模型 API 连接以启用 AI 健康报告功能。\n"
        "支持任意 OpenAI 兼容接口（DeepSeek、OpenAI、Ollama 等）。\n"
        "API Key 留空时自动从环境变量 OPENAI_API_KEY 读取。");
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("color: #666; margin-bottom: 8px;");
    mainLayout->addWidget(hintLabel);

    // ---- 配置表单 ----
    auto* formGroup = new QGroupBox("API 连接参数");
    auto* formLayout = new QFormLayout(formGroup);

    // Endpoint
    endpointEdit_ = new QLineEdit();
    endpointEdit_->setPlaceholderText("https://api.deepseek.com/chat/completions");
    endpointEdit_->setText("https://api.deepseek.com/chat/completions");
    formLayout->addRow("Endpoint:", endpointEdit_);

    // API Key
    apiKeyEdit_ = new QLineEdit();
    apiKeyEdit_->setPlaceholderText("sk-...（留空则读环境变量）");
    apiKeyEdit_->setEchoMode(QLineEdit::Password);  // 密码遮罩
    formLayout->addRow("API Key:", apiKeyEdit_);

    // Model
    modelEdit_ = new QLineEdit();
    modelEdit_->setPlaceholderText("deepseek-v4-pro");
    modelEdit_->setText("deepseek-v4-pro");
    formLayout->addRow("Model:", modelEdit_);

    mainLayout->addWidget(formGroup);

    // ---- 状态标签 ----
    statusLabel_ = new QLabel();
    statusLabel_->setWordWrap(true);
    statusLabel_->setStyleSheet("padding: 6px; font-weight: bold;");
    mainLayout->addWidget(statusLabel_);

    // ---- 按钮 ----
    auto* buttonLayout = new QHBoxLayout();

    auto* testBtn = new QPushButton("测试连接");
    connect(testBtn, &QPushButton::clicked, this, &AISettingsDialog::onTestClicked);
    buttonLayout->addWidget(testBtn);

    buttonLayout->addStretch();

    auto* cancelBtn = new QPushButton("取消");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelBtn);

    auto* saveBtn = new QPushButton("保存");
    saveBtn->setDefault(true);
    saveBtn->setStyleSheet("QPushButton { font-weight: bold; }");
    connect(saveBtn, &QPushButton::clicked, this, &AISettingsDialog::onSaveClicked);
    buttonLayout->addWidget(saveBtn);

    mainLayout->addLayout(buttonLayout);
}

// ============================================================
// 更新状态指示
// ============================================================
void AISettingsDialog::updateStatusLabel()
{
    if (!manager_) {
        statusLabel_->setText("❌ 后端未连接");
        statusLabel_->setStyleSheet("color: red; padding: 6px; font-weight: bold;");
        return;
    }
    if (manager_->isLLMConfigured()) {
        statusLabel_->setText("✅ AI 顾问已配置，可以生成 AI 报告");
        statusLabel_->setStyleSheet("color: green; padding: 6px; font-weight: bold;");
    } else {
        statusLabel_->setText("⚠️ 尚未配置 API Key，AI 报告将降级为本地报告");
        statusLabel_->setStyleSheet("color: #e67e00; padding: 6px; font-weight: bold;");
    }
}

// ============================================================
// 保存
// ============================================================
void AISettingsDialog::onSaveClicked()
{
    if (!manager_) {
        QMessageBox::warning(this, "错误", "后端服务未连接");
        return;
    }

    QString endpoint = endpointEdit_->text().trimmed();
    QString apiKey   = apiKeyEdit_->text().trimmed();
    QString model    = modelEdit_->text().trimmed();

    if (endpoint.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入 API Endpoint");
        endpointEdit_->setFocus();
        return;
    }

    bool ok = manager_->configureLLM(
        endpoint.toStdString(),
        apiKey.toStdString(),
        model.isEmpty() ? "deepseek-v4-pro" : model.toStdString());

    if (ok) {
        QMessageBox::information(this, "成功", "AI 顾问配置已保存！");
        updateStatusLabel();
        accept();  // 关闭对话框，返回 Accepted
    } else {
        QMessageBox::warning(this, "失败",
            "配置失败。请检查：\n"
            "1. API Key 是否正确（或已设置环境变量 OPENAI_API_KEY）\n"
            "2. Endpoint URL 格式是否正确");
        updateStatusLabel();
    }
}

// ============================================================
// 测试连接（快速验证：生成一份简短 AI 报告）
// ============================================================
void AISettingsDialog::onTestClicked()
{
    if (!manager_) return;

    // 先生效当前填写的配置
    QString endpoint = endpointEdit_->text().trimmed();
    QString apiKey   = apiKeyEdit_->text().trimmed();
    QString model    = modelEdit_->text().trimmed();

    if (endpoint.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先填写 Endpoint");
        return;
    }

    bool configured = manager_->configureLLM(
        endpoint.toStdString(),
        apiKey.toStdString(),
        model.isEmpty() ? "deepseek-v4-pro" : model.toStdString());

    if (!configured) {
        QMessageBox::warning(this, "失败", "配置失败，请检查 API Key");
        updateStatusLabel();
        return;
    }

    // 尝试调用 AI 生成一份简约周报
    QMessageBox::information(this, "测试中",
        "正在测试 AI 连接...\n这可能需要 10-30 秒，请耐心等待。");

    std::string report = manager_->generateAIReport(
        health::HealthManager::ReportPeriod::WEEKLY);

    if (report.find("\"error\"") != std::string::npos &&
        report.find("\"error\":") != std::string::npos) {
        // 提取错误详情
        QString errMsg = "AI API 调用失败。\n";
        auto errJson = QJsonDocument::fromJson(QString::fromStdString(report).toUtf8());
        if (errJson.isObject() && errJson.object().contains("error")) {
            errMsg += "错误: " + errJson.object()["error"].toString();
        }
        QMessageBox::warning(this, "测试失败", errMsg);
    } else {
        QMessageBox::information(this, "测试通过",
            "✅ AI 连接正常！可以正常使用 AI 报告功能。\n\n"
            "提示：如果后续 AI 报告仍出现问题，请检查网络或 API 配额。");
    }
    updateStatusLabel();
}
