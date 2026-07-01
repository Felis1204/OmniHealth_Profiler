#pragma once

#include <QWidget>
#include <memory>

namespace health { class HealthManager; }

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;

private slots:
    void on_pushButton_clicked();
    void on_AddDataButton_clicked();
    void on_AddDataButton_2_clicked();
    void on_usermanagebutton_clicked();

    // 输出功能按钮（远程）
    void on_dashboardButton_clicked();
    void on_trendButton_clicked();
    void on_reportButton_clicked();
    void on_aiButton_clicked();

    // AI 功能（本地新增）
    void on_aiReportButton_clicked();
    void on_aiSettingsButton_clicked();
    void on_localReportButton_clicked();
    void on_riskButton_clicked();

private:
    Ui::Widget *ui;
    std::unique_ptr<health::HealthManager> manager_;
};
