#include "AIReportDialog.h"
#include "HealthManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QScrollBar>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>

AIReportDialog::AIReportDialog(health::HealthManager* mgr,
                               QWidget* parent)
    : QDialog(parent)
    , manager_(mgr)
{
    buildUI();

    // 启动时检查 AI 配置状态
    if (manager_ && !manager_->isLLMConfigured()) {
        appendMessage("系统", "⚠️ AI 顾问未配置。点击「生成报告」将使用本地降级报告。\n"
                             "如需 AI 功能，请先在主界面 → AI 设置中配置 API Key。");
    }
}

// ============================================================
// 构建 UI
// ============================================================
void AIReportDialog::buildUI()
{
    setWindowTitle("AI 健康顾问");
    resize(720, 660);
    setMinimumSize(520, 480);

    auto* mainLayout = new QVBoxLayout(this);

    // ---- 顶部：报告类型选择 + 生成按钮 ----
    auto* topLayout = new QHBoxLayout();

    topLayout->addWidget(new QLabel("报告周期:"));

    periodCombo_ = new QComboBox();
    periodCombo_->addItem("📅 周报（近 7 天）",
        static_cast<int>(health::HealthManager::ReportPeriod::WEEKLY));
    periodCombo_->addItem("📅 月报（近 30 天）",
        static_cast<int>(health::HealthManager::ReportPeriod::MONTHLY));
    topLayout->addWidget(periodCombo_);

    generateBtn_ = new QPushButton("生成报告");
    generateBtn_->setAutoDefault(false);   // 防止 Enter 键误触发
    generateBtn_->setStyleSheet(
        "QPushButton { font-weight: bold; padding: 6px 20px; }");
    connect(generateBtn_, &QPushButton::clicked,
            this, &AIReportDialog::onGenerateClicked);
    topLayout->addWidget(generateBtn_);

    topLayout->addStretch();

    clearBtn_ = new QPushButton("清空对话");
    clearBtn_->setAutoDefault(false);
    connect(clearBtn_, &QPushButton::clicked,
            this, &AIReportDialog::onClearChatClicked);
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
        "QTextBrowser { background: #fafafa; color: #333; "
        "border: 1px solid #ddd; border-radius: 6px; padding: 10px; "
        "font-size: 13px; }");
    mainLayout->addWidget(chatBrowser_, 1);  // stretch=1 占据剩余空间

    // ---- 底部：追问输入区 ----
    auto* bottomLayout = new QHBoxLayout();

    questionEdit_ = new QLineEdit();
    questionEdit_->setPlaceholderText("输入追问...如：「我的血糖偏高应该怎么办？」");
    questionEdit_->setEnabled(false);
    connect(questionEdit_, &QLineEdit::returnPressed,
            this, &AIReportDialog::onSendFollowUpClicked);
    bottomLayout->addWidget(questionEdit_, 1);

    sendBtn_ = new QPushButton("发送");
    sendBtn_->setAutoDefault(false);   // 防止 Enter 键误触发
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

    // 禁用按钮防止重复点击（用 repaint 而非 processEvents，避免事件重入）
    generateBtn_->setEnabled(false);
    statusLabel_->setText("正在请求 AI...");
    statusLabel_->repaint();

    // 调用后端（AI-First：成功则 AI JSON，失败则降级为本地 text）
    std::string report = manager_->generateAIReport(period);

    // 恢复按钮
    generateBtn_->setEnabled(true);

    // 尝试格式化为结构化报告
    QString formatted = formatAIReport(report);

    if (!formatted.isEmpty()) {
        // AI 报告 JSON 解析成功 → 显示结构化内容
        statusLabel_->setText("✅ AI " + periodLabel + " 生成成功 — 可追问");
        statusLabel_->setStyleSheet(
            "color: green; font-weight: bold; padding: 2px 8px;");
        hasReportContext_ = true;
        setInputEnabled(true);
        appendMessage("AI 顾问", formatted, true);  // 结构化 HTML
    } else {
        // 降级或本地报告 → 判断具体原因
        bool isError = (report.find("error") != std::string::npos &&
                        report.find("\"error\"") != std::string::npos);
        bool isPlainText = !report.empty() && report[0] != '{';

        if (isPlainText) {
            // 本地降级报告（纯文本格式）
            statusLabel_->setText("📋 本地" + periodLabel + "（AI 未配置/不可用）");
            statusLabel_->setStyleSheet(
                "color: #e67e00; font-weight: bold; padding: 2px 8px;");
            hasReportContext_ = false;
            setInputEnabled(false);
            appendMessage("本地报告", QString::fromStdString(report));
        } else if (isError) {
            statusLabel_->setText("⚠️ AI 服务暂不可用，已降级为本地报告");
            statusLabel_->setStyleSheet(
                "color: #e67e00; font-weight: bold; padding: 2px 8px;");
            hasReportContext_ = false;
            setInputEnabled(false);
            appendMessage("系统", "❌ AI 服务返回错误，请检查 API 配置或稍后重试。\n\n"
                                "原始响应:\n" + QString::fromStdString(report));
        } else {
            statusLabel_->setText("⚠️ 报告格式异常，请重试");
            statusLabel_->setStyleSheet(
                "color: red; font-weight: bold; padding: 2px 8px;");
            hasReportContext_ = false;
            setInputEnabled(false);
            appendMessage("系统", QString::fromStdString(report));
        }
    }

    // 滚动到底部
    chatBrowser_->verticalScrollBar()->setValue(
        chatBrowser_->verticalScrollBar()->maximum());
}

// ============================================================
// formatAIReport — 解析 AI 响应为结构化 HTML
// ============================================================
QString AIReportDialog::formatAIReport(const std::string& rawResponse)
{
    if (rawResponse.empty()) return {};

    QString text = QString::fromStdString(rawResponse).trimmed();
    if (text.isEmpty()) return {};

    // 清理 LLM 可能包裹的 markdown 代码块标记
    text = stripMarkdownCodeBlock(text);

    // 尝试解析 JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};  // 非 JSON → 降级报告
    }

    QJsonObject root = doc.object();

    // 检查核心字段是否存在
    if (!root.contains("risk_analysis") && !root.contains("risk_assessment")
        && !root.contains("keys_cn")) {
        if (root.contains("error")) {
            return {};  // 错误 JSON
        }
        return {};
    }

    return renderReportHtml(root);
}

// ============================================================
// renderReportHtml — 将 JSON 渲染为结构化 HTML
// ============================================================
QString AIReportDialog::renderReportHtml(const QJsonObject& root)
{
    auto esc = [](const QString& s) {
        return s.toHtmlEscaped().replace("\n", "<br>");
    };

    auto renderList = [&](const QJsonArray& arr) -> QString {
        QString html;
        for (const auto& item : arr) {
            html += "<li>" + esc(item.toString()) + "</li>";
        }
        return html;
    };

    QJsonObject riskAna;
    if (root.contains("risk_analysis"))
        riskAna = root["risk_analysis"].toObject();
    else if (root.contains("risk_assessment"))
        riskAna = root["risk_assessment"].toObject();

    QJsonObject actionPlan = root["action_plan"].toObject();
    QString conclusion = root["conclusion"].toString();
    QString followUp   = root["follow_up_prompt"].toString();
    QString disclaimer = root["disclaimer"].toString();

    if (root.contains("keys_cn")) {
        QJsonArray keysCn = root["keys_cn"].toArray();
        if (!keysCn.isEmpty()) {
            conclusion = "关键指标: ";
            for (const auto& k : keysCn)
                conclusion += k.toString() + "  ";
        }
    }

    QString html;

    // ===== 1. 风险分析 =====
    if (!riskAna.isEmpty()) {
        html += "<div style='background:#fff3e0; border-left:4px solid #e65100; "
                "padding:12px 16px; margin-bottom:12px; border-radius:4px;'>";
        html += "<h3 style='color:#e65100; margin-top:0;'>🩺 风险分析</h3>";

        QString overall = riskAna["overall"].toString();
        if (!overall.isEmpty())
            html += "<p style='line-height:1.8;'><b>📋 综合评估：</b>"
                    + esc(overall) + "</p>";

        QString cardio = riskAna["cardiovascular"].toString();
        if (!cardio.isEmpty())
            html += "<p style='line-height:1.8;'><b>❤️ 心血管专项：</b>"
                    + esc(cardio) + "</p>";

        QString metabolic = riskAna["metabolic"].toString();
        if (!metabolic.isEmpty())
            html += "<p style='line-height:1.8;'><b>🧬 代谢专项：</b>"
                    + esc(metabolic) + "</p>";

        QJsonArray alerts = riskAna["alert_items"].toArray();
        if (!alerts.isEmpty()) {
            html += "<p><b>🚨 警示项目：</b></p><ul style='line-height:1.8;'>";
            html += renderList(alerts);
            html += "</ul>";
        }

        html += "</div>";
    }

    // ===== 2. 行动计划 =====
    if (!actionPlan.isEmpty()) {
        html += "<div style='background:#e8f5e9; border-left:4px solid #2e7d32; "
                "padding:12px 16px; margin-bottom:12px; border-radius:4px;'>";
        html += "<h3 style='color:#2e7d32; margin-top:0;'>📋 行动计划</h3>";

        auto addSection = [&](const QString& icon, const QString& title,
                               const QJsonArray& arr) {
            if (!arr.isEmpty()) {
                html += "<p><b>" + icon + " " + title + "：</b></p>"
                        "<ul style='line-height:1.8;'>"
                        + renderList(arr) + "</ul>";
            }
        };
        addSection("🥗", "饮食建议", actionPlan["diet"].toArray());
        addSection("🏃", "运动建议", actionPlan["exercise"].toArray());
        addSection("🌿", "生活方式", actionPlan["lifestyle"].toArray());
        addSection("📊", "监测建议", actionPlan["monitoring"].toArray());

        html += "</div>";
    }

    // ===== 3. 结论 =====
    if (!conclusion.isEmpty()) {
        html += "<div style='background:#e3f2fd; border-left:4px solid #1565c0; "
                "padding:12px 16px; margin-bottom:12px; border-radius:4px;'>";
        html += "<h3 style='color:#1565c0; margin-top:0;'>💡 核心结论</h3>";
        html += "<p style='line-height:1.8;'>" + esc(conclusion) + "</p>";
        html += "</div>";
    }

    // ===== 4. 追问引导 =====
    if (!followUp.isEmpty()) {
        html += "<div style='background:#f3e5f5; border-left:4px solid #7b1fa2; "
                "padding:12px 16px; margin-bottom:12px; border-radius:4px;'>";
        html += "<p style='line-height:1.8; margin:0; color:#7b1fa2;'>"
                + esc(followUp) + "</p>";
        html += "</div>";
    }

    // ===== 5. 免责声明 =====
    if (!disclaimer.isEmpty()) {
        html += "<div style='color:#999; font-size:11px; padding:8px 12px; "
                "margin-top:8px; border-top:1px solid #eee;'>"
                + esc(disclaimer) + "</div>";
    }

    return html;
}

// ============================================================
// stripMarkdownCodeBlock — 清理 ```json ... ``` 包裹
// ============================================================
QString AIReportDialog::stripMarkdownCodeBlock(const QString& text)
{
    QString t = text.trimmed();

    QRegularExpression re(R"(^```(?:json|JSON)?\s*\n(.*?)\n```$)",
                          QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch match = re.match(t);
    if (match.hasMatch())
        return match.captured(1).trimmed();

    if (t.startsWith("```")) {
        QString stripped = t;
        stripped.replace(QRegularExpression("^```(?:json|JSON)?\\s*"), "");
        stripped.replace(QRegularExpression("\\s*```$"), "");
        return stripped.trimmed();
    }

    return t;
}

// ============================================================
// markdownToHtml — 将追问回复的 markdown 转为 HTML
// ============================================================
QString AIReportDialog::markdownToHtml(const QString& md)
{
    QString html;
    html.reserve(md.size() * 2);

    // 先按行拆分
    QStringList lines = md.split('\n');
    bool inTable = false;
    bool inUl = false;
    bool inOl = false;

    auto closeLists = [&]() {
        if (inUl) { html += "</ul>"; inUl = false; }
        if (inOl) { html += "</ol>"; inOl = false; }
    };

    auto closeTable = [&]() {
        if (inTable) { html += "</table>"; inTable = false; }
    };

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];

        // --- 水平分隔线 ---
        if (QRegularExpression(R"(^-{3,}\s*$)").match(line).hasMatch()) {
            closeLists();
            closeTable();
            html += "<hr style='border:none; border-top:1px solid #ddd; margin:12px 0;'>";
            continue;
        }

        // --- 表格 ---
        if (line.startsWith('|') && line.endsWith('|')) {
            closeLists();
            // 跳过分隔行 (| --- | --- |)
            if (QRegularExpression(R"(^\|[\s\-:]+\|)").match(line).hasMatch())
                continue;

            QStringList cells = line.split('|');
            // 去掉首尾空元素
            if (!cells.isEmpty() && cells.first().trimmed().isEmpty())
                cells.removeFirst();
            if (!cells.isEmpty() && cells.last().trimmed().isEmpty())
                cells.removeLast();

            if (!inTable) {
                html += "<table style='border-collapse:collapse; margin:8px 0; "
                        "width:100%;'>";
                inTable = true;
            }

            // 判断是否表头行（下一行是分隔行）
            bool isHeader = (i + 1 < lines.size() &&
                QRegularExpression(R"(^\|[\s\-:]+\|)").match(lines[i + 1]).hasMatch());

            QString tag = isHeader ? "th" : "td";
            QString cellStyle = isHeader
                ? "style='background:#f0f0f0; font-weight:bold; padding:6px 10px; "
                  "border:1px solid #ddd; text-align:left;'"
                : "style='padding:6px 10px; border:1px solid #ddd;'";

            html += "<tr>";
            for (const QString& cell : cells) {
                html += "<" + tag + " " + cellStyle + ">"
                        + inlineMarkdown(cell.trimmed()) + "</" + tag + ">";
            }
            html += "</tr>";
            continue;
        } else {
            closeTable();
        }

        // --- 标题 ---
        QRegularExpression headerRe(R"(^(#{1,4})\s+(.+))");
        QRegularExpressionMatch hm = headerRe.match(line);
        if (hm.hasMatch()) {
            closeLists();
            int level = hm.captured(1).size();
            QString text = inlineMarkdown(hm.captured(2));
            int fontSize = (level == 1) ? 18 : (level == 2) ? 16 : (level == 3) ? 14 : 13;
            html += QString("<h%1 style='font-size:%2px; margin:12px 0 4px 0; "
                            "color:#333;'>%3</h%1>")
                        .arg(level + 2).arg(fontSize).arg(text);
            continue;
        }

        // --- 无序列表 ---
        QRegularExpression ulRe(R"(^(\s*)[-*+]\s+(.+))");
        QRegularExpressionMatch um = ulRe.match(line);
        if (um.hasMatch()) {
            if (!inUl) { closeLists(); html += "<ul style='line-height:1.8;'>"; inUl = true; }
            html += "<li>" + inlineMarkdown(um.captured(2)) + "</li>";
            continue;
        }

        // --- 有序列表 ---
        QRegularExpression olRe(R"(^(\s*)\d+\.\s+(.+))");
        QRegularExpressionMatch om = olRe.match(line);
        if (om.hasMatch()) {
            if (!inOl) { closeLists(); html += "<ol style='line-height:1.8;'>"; inOl = true; }
            html += "<li>" + inlineMarkdown(om.captured(2)) + "</li>";
            continue;
        }

        // 列表结束
        closeLists();

        // --- 空行 ---
        if (line.trimmed().isEmpty()) {
            html += "<br>";
            continue;
        }

        // --- 引用 ---
        if (line.startsWith('>')) {
            QString text = line.mid(1).trimmed();
            html += "<blockquote style='border-left:3px solid #ccc; padding:4px 12px; "
                    "margin:8px 0; color:#666;'>" + inlineMarkdown(text) + "</blockquote>";
            continue;
        }

        // --- 普通段落 ---
        html += "<p style='line-height:1.8; margin:4px 0;'>"
                + inlineMarkdown(line) + "</p>";
    }

    closeLists();
    closeTable();

    return html;
}

// ============================================================
// inlineMarkdown — 行内格式：**粗体** *斜体* `代码`
// ============================================================
QString AIReportDialog::inlineMarkdown(const QString& text)
{
    QString result = text.toHtmlEscaped();

    // **粗体**
    result.replace(QRegularExpression(R"(\*\*(.+?)\*\*)"), "<b>\\1</b>");
    // *斜体*（注意不要跟 ** 冲突，因为已经处理过 ** 了）
    result.replace(QRegularExpression(R"(?<!\*)\*(?!\*)(.+?)(?<!\*)\*(?!\*)"), "<i>\\1</i>");
    // `代码`
    result.replace(QRegularExpression(R"(`([^`]+)`)"),
                   "<code style='background:#f0f0f0; padding:1px 4px; "
                   "border-radius:3px; font-family:monospace;'>\\1</code>");

    return result;
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

    // 发送追问（用 repaint 而非 processEvents，避免 Enter 键事件重入触发默认按钮）
    sendBtn_->setEnabled(false);
    questionEdit_->setEnabled(false);
    statusLabel_->setText("AI 正在回复...");
    statusLabel_->setStyleSheet(
        "color: #888; font-weight: bold; padding: 2px 8px;");
    statusLabel_->repaint();

    std::string answer = manager_->askFollowUp(question.toStdString());

    sendBtn_->setEnabled(true);
    questionEdit_->setEnabled(true);
    questionEdit_->setFocus();

    if (answer.find("AI 顾问未配置") != std::string::npos ||
        answer.find("尚未生成健康报告") != std::string::npos) {
        statusLabel_->setText("⚠️ 追问失败");
        statusLabel_->setStyleSheet(
            "color: red; font-weight: bold; padding: 2px 8px;");
        hasReportContext_ = false;
        setInputEnabled(false);
    } else if (answer.find("AI 服务暂时不可用") != std::string::npos) {
        statusLabel_->setText("⚠️ AI 服务暂不可用，稍后重试");
        statusLabel_->setStyleSheet(
            "color: #e67e00; font-weight: bold; padding: 2px 8px;");
    } else {
        statusLabel_->setText("✅ AI 回复成功");
        statusLabel_->setStyleSheet(
            "color: green; font-weight: bold; padding: 2px 8px;");
    }

    // 追问回复是 markdown 格式，转为 HTML 后渲染
    QString htmlAnswer = markdownToHtml(QString::fromStdString(answer));
    appendMessage("AI 顾问", htmlAnswer, true);

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
    statusLabel_->setText("对话已清空。点击「生成报告」开始新一轮分析。");
    statusLabel_->setStyleSheet("color: #888; padding: 2px 8px;");

    if (manager_ && !manager_->isLLMConfigured()) {
        appendMessage("系统", "⚠️ AI 顾问未配置，将使用本地降级报告。");
    }
}

// ============================================================
// 辅助方法
// ============================================================
void AIReportDialog::appendMessage(const QString& role, const QString& text,
                                     bool isHtml)
{
    QString color;
    if (role == "AI 顾问")       color = "#1565c0";
    else if (role == "你")       color = "#2e7d32";
    else if (role == "本地报告")   color = "#e65100";
    else                         color = "#888";

    QString content;
    if (isHtml) {
        content = text;
    } else {
        content = text.toHtmlEscaped();
        content.replace("\n", "<br>");
    }

    chatBrowser_->append(
        QString("<p style='color:%1; margin:6px 0 2px 0;'><b>[%2]</b></p>"
                "<div style='margin:0 0 14px 8px;'>%3</div>")
            .arg(color, role, content));
}

void AIReportDialog::setInputEnabled(bool enabled)
{
    questionEdit_->setEnabled(enabled);
    sendBtn_->setEnabled(enabled);
    if (enabled) {
        questionEdit_->setPlaceholderText(
            "输入追问...如：「我的血糖偏高应该怎么办？」");
    } else {
        questionEdit_->setPlaceholderText("请先生成 AI 报告");
    }
}
