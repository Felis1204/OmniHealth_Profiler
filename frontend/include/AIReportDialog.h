#pragma once

#include <QDialog>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QJsonObject>

namespace health { class HealthManager; }

/// @brief AI 健康报告与追问对话框
///
/// 提供两个功能：
///   1. 生成 AI 健康报告（周报/月报），以结构化 HTML 展示
///   2. 追问交互：在下方的输入框中输入问题，AI 结合健康上下文回复
///
/// AI-First 策略（由后端保证）：
///   - LLM 已配置 → 调用 DeepSeek API 生成 JSON 格式个性化报告
///   - LLM 未配置 → 自动降级为本地 text 报告
///   - API 调用失败 → 自动降级，显示"AI 暂不可用"
///
/// 使用示例：
/// @code
///   AIReportDialog dlg(manager_.get(), this);
///   dlg.exec();
/// @endcode
class AIReportDialog : public QDialog
{
    Q_OBJECT

public:
    /// @param mgr    HealthManager 指针（非空）
    /// @param parent 父窗口
    explicit AIReportDialog(health::HealthManager* mgr,
                            QWidget* parent = nullptr);
    ~AIReportDialog() override = default;

private slots:
    void onGenerateClicked();
    void onSendFollowUpClicked();
    void onClearChatClicked();

private:
    void buildUI();
    void appendMessage(const QString& role, const QString& text,
                       bool isHtml = false);
    void setInputEnabled(bool enabled);

    /// @brief 尝试将 AI 响应解析为 JSON 并格式化为结构化 HTML
    /// @param rawResponse 后端返回的原始字符串
    /// @return 格式化后的 HTML；若 JSON 解析失败则返回空字符串（调用方应显示原文）
    QString formatAIReport(const std::string& rawResponse);

    /// @brief 将 JSON 对象的某段渲染为 HTML 报告
    QString renderReportHtml(const QJsonObject& root);

    /// @brief 清理 LLM 可能包裹的 markdown 代码块标记
    static QString stripMarkdownCodeBlock(const QString& text);

    /// @brief 将追问回复的 markdown 文本转为 HTML（粗体/斜体/标题/列表/表格/代码）
    static QString markdownToHtml(const QString& md);

    /// @brief 行内 markdown 转换：**粗体** *斜体* `代码`，含 HTML 转义
    static QString inlineMarkdown(const QString& text);

    health::HealthManager* manager_;
    bool hasReportContext_ = false;   // 是否已成功生成过报告（用于追问前提校验）

    // UI 控件
    QComboBox*     periodCombo_;
    QPushButton*   generateBtn_;
    QTextBrowser*  chatBrowser_;
    QLineEdit*     questionEdit_;
    QPushButton*   sendBtn_;
    QPushButton*   clearBtn_;
    QLabel*        statusLabel_;
};
