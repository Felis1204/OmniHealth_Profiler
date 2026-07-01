#include "AIReportDialog.h"
#include "HealthManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QMessageBox>
#include <QScrollBar>
#include <QApplication>

AIReportDialog::AIReportDialog(health::HealthManager* mgr,
                               QWidget* parent)
    : QDialog(parent)
    , manager_(mgr)
{
    buildUI();

    // 启动时检查 AI 配置状态
    if (manager_ && !manager_->isLLMConfigured()) {
        appendMessage("系统", "⚠️ AI 顾问未配置。点击\"生成报告\"将使用本地降级报告。\n"
                             "如需 AI 功能，请先在主界面 → AI 设置中配置 API Key。");
    }
}

// ============================================================
// 构建 UI
// ============================================================
void AIReportDialog::buildUI()
{
    setWindowTitle("AI 健康顾问");
    resize(680, 620);
    setMinimumSize(520, 480);

    auto* mainLayout = new QVBoxLayout(this);

    // ---- 顶部：报告类型选择 + 生成按钮 ----
    auto* topLayout = new QHBoxLayout();

    topLayout->addWidget(new QLabel("报告周期:"));

    periodCombo_ = new QComboBox();
    periodCombo_->addItem("📅 周报（近 7 天）", static_cast<int>(health::HealthManager::ReportPeriod::WEEKLY));
    periodCombo_->addItem("📅 月报（近 30 天）", static_cast<int>(health::HealthManager::ReportPeriod::MONTHLY));
    topLayout->addWidget(periodCombo_);

    generateBtn_ = new QPushButton("生成报告");
    generateBtn_->setStyleSheet("QPushButton { font-weight: bold; padding: 6px 20px; }");
    connect(generateBtn_, &QPushButton::clicked, this, &AIReportDialog::onGenerateClicked);
    topLayout->addWidget(generateBtn_);

    topLayout->addStretch();

    clearBtn_ = new QPushButton("清空对话");
    connect(clearBtn_, &QPushButton::clicked, this, &AIReportDialog::onClearChatClicked);
    topLayout->addWidget(clearBtn_);

    mainLayout->addLayout(topLayout);

    // ---- 状态标签 ----
    statusLabel_ = new QLabel();
    statusLabel_->setStyleSheet("color: #888; padding: 2px 8px;");
    mainLayout->addWidget(statusLabel_);

    // ---- 中间：对话/报告展示区 ----
    chatBrowser_ = new QTextBrowser();
    chatBrowser_->setOpenExternalLinks(true);
    chatBrowser_->setStyleSheet(
        "QTextBrowser { background: #1e1e1e; color: #e0e0e0; "
        "border: 1px solid #444; border-radius: 4px; padding: 8px; "
        "font-size: 13px; }");
    mainLayout->addWidget(chatBrowser_, 1);  // stretch=1 占据剩余空间

    // ---- 底部：追问输入区 ----
    auto* bottomLayout = new QHBoxLayout();

    questionEdit_ = new QLineEdit();
    questionEdit_->setPlaceholderText("输入追问...如：\"我的血糖偏高应该怎么办？\"");
    questionEdit_->setEnabled(false);
    connect(questionEdit_, &QLineEdit::returnPressed,
            this, &AIReportDialog::onSendFollowUpClicked);
    bottomLayout->addWidget(questionEdit_, 1);

    sendBtn_ = new QPushButton("发送");
    sendBtn_->setEnabled(false);
    connect(sendBtn_, &QPushButton::clicked,
            this, &AIReportDialog::onSendFollowUpClicked);
    bottomLayout->addWidget(sendBtn_);

    mainLayout->addLayout(bottomLayout);
}

// ============================================================
// 生成报告
// ============================================================
void AIReportDialog::onGenerateClicked()
{
    if (!manager_) {
        QMessageBox::warning(this, "错误", "后端服务未连接");
        return;
    }

    auto period = static_cast<health::HealthManager::ReportPeriod>(
        periodCombo_->currentData().toInt());

    QString periodLabel = (period == health::HealthManager::ReportPeriod::WEEKLY)
                          ? "周报" : "月报";

    appendMessage("系统", "⏳ 正在生成" + periodLabel + "，请稍候...");

    // 禁用按钮防止重复点击
    generateBtn_->setEnabled(false);
    statusLabel_->setText("正在请求 AI...");
    QApplication::processEvents();

    // 调用后端（AI-First：成功则 AI JSON，失败则降级为本地 text）
    std::string report = manager_->generateAIReport(period);

    // 恢复按钮
    generateBtn_->setEnabled(true);

    // 判断是否来自 AI 还是降级
    bool isAI = (report.find("\"risk_analysis\"") != std::string::npos ||
                 report.find("\"risk_assessment\"") != std::string::npos ||
                 report.find("\"keys_cn\"") != std::string::npos);

    if (isAI) {
        statusLabel_->setText("✅ AI " + periodLabel + " 生成成功 — 可追问");
        hasReportContext_ = true;
        setInputEnabled(true);
        appendMessage("AI 顾问", QString::fromStdString(report));
    } else {
        // 降级或本地报告
        bool isError = (report.find("AI 服务暂不可用") != std::string::npos);
        if (isError) {
            statusLabel_->setText("⚠️ AI 服务暂不可用，已降级为本地报告");
            // 降级情况下仍可尝试追问（如果之前有上下文）
            // 这里清除上下文因为没有成功缓存
            hasReportContext_ = false;
            setInputEnabled(false);
        } else {
            statusLabel_->setText("📋 本地" + periodLabel + "（AI 未配置/不可用）");
            hasReportContext_ = false;
            setInputEnabled(false);
        }
        appendMessage("本地报告", QString::fromStdString(report));
    }

    // 滚动到底部
    chatBrowser_->verticalScrollBar()->setValue(
        chatBrowser_->verticalScrollBar()->maximum());
}

// ============================================================
// 追问
// ============================================================
void AIReportDialog::onSendFollowUpClicked()
{
    if (!manager_) return;

    QString question = questionEdit_->text().trimmed();
    if (question.isEmpty()) return;

    if (!hasReportContext_) {
        QMessageBox::information(this, "提示",
            "请先生成一份 AI 健康报告，然后才能进行追问。");
        return;
    }

    // 显示用户问题
    appendMessage("你", question);
    questionEdit_->clear();

    // 发送追问
    sendBtn_->setEnabled(false);
    questionEdit_->setEnabled(false);
    statusLabel_->setText("AI 正在回复...");
    QApplication::processEvents();

    std::string answer = manager_->askFollowUp(question.toStdString());

    sendBtn_->setEnabled(true);
    questionEdit_->setEnabled(true);
    questionEdit_->setFocus();

    if (answer.find("AI 顾问未配置") != std::string::npos ||
        answer.find("尚未生成健康报告") != std::string::npos) {
        statusLabel_->setText("⚠️ 追问失败");
        hasReportContext_ = false;
        setInputEnabled(false);
    } else if (answer.find("AI 服务暂时不可用") != std::string::npos) {
        statusLabel_->setText("⚠️ AI 服务暂不可用，稍后重试");
    } else {
        statusLabel_->setText("✅ AI 回复成功");
    }

    appendMessage("AI 顾问", QString::fromStdString(answer));

    chatBrowser_->verticalScrollBar()->setValue(
        chatBrowser_->verticalScrollBar()->maximum());
}

// ============================================================
// 清空对话
// ============================================================
void AIReportDialog::onClearChatClicked()
{
    chatBrowser_->clear();
    hasReportContext_ = false;
    setInputEnabled(false);
    statusLabel_->setText(QString::fromUtf8("对话已清空。点击「生成报告」开始新一轮分析。"));

    if (manager_ && !manager_->isLLMConfigured()) {
        appendMessage("系统", "⚠️ AI 顾问未配置，将使用本地降级报告。");
    }
}

// ============================================================
// 辅助方法
// ============================================================
void AIReportDialog::appendMessage(const QString& role, const QString& text)
{
    // 不同角色用不同颜色
    QString color;
    if (role == "AI 顾问") {
        color = "#4fc3f7";  // 亮蓝
    } else if (role == "你") {
        color = "#a5d6a7";  // 亮绿
    } else if (role == "本地报告") {
        color = "#ffcc80";  // 橙色
    } else {
        color = "#999";     // 灰色（系统消息）
    }

    chatBrowser_->append(
        QString("<p style='color:%1; margin:4px 0;'><b>[%2]</b></p>"
                "<p style='color:#e0e0e0; margin:0 0 12px 12px; white-space:pre-wrap;'>%3</p>")
            .arg(color, role, text));
}

void AIReportDialog::setInputEnabled(bool enabled)
{
    questionEdit_->setEnabled(enabled);
    sendBtn_->setEnabled(enabled);
    if (enabled) {
        questionEdit_->setPlaceholderText("输入追问...如：\"我的血糖偏高应该怎么办？\"");
    } else {
        questionEdit_->setPlaceholderText("请先生成 AI 报告");
    }
}
