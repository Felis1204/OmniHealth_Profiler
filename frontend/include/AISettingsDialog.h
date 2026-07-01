#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

namespace health { class HealthManager; }

/// @brief AI API 配置对话框
///
/// 用户在此输入 LLM API 的连接参数：
///   - Endpoint: OpenAI 兼容的 chat completions URL
///   - API Key:  密钥（留空则从环境变量 OPENAI_API_KEY 读取）
///   - Model:    模型名称（默认 deepseek-v4-pro）
///
/// 点击"保存"后调用 HealthManager::configureLLM() 持久化配置。
///
/// 使用示例：
/// @code
///   AISettingsDialog dlg(manager_.get(), this);
///   if (dlg.exec() == QDialog::Accepted) {
///       // 配置已保存，AI 功能可用
///   }
/// @endcode
class AISettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /// @param mgr    HealthManager 指针（非空）
    /// @param parent 父窗口
    explicit AISettingsDialog(health::HealthManager* mgr,
                              QWidget* parent = nullptr);
    ~AISettingsDialog() override = default;

private slots:
    void onSaveClicked();
    void onTestClicked();

private:
    void buildUI();
    void updateStatusLabel();

    health::HealthManager* manager_;

    QLineEdit* endpointEdit_;
    QLineEdit* apiKeyEdit_;
    QLineEdit* modelEdit_;
    QLabel*    statusLabel_;
};
