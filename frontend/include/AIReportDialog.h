#pragma once

#include <QDialog>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>

namespace health { class HealthManager; }

/// @brief AI 健康报告与追问对话框
///
/// 提供两个功能：
///   1. 生成 AI 健康报告（周报/月报），在 textBrowser 中展示
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
    void appendMessage(const QString& role, const QString& text);
    void setInputEnabled(bool enabled);

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
