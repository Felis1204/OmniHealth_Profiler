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
    // 个人档案
    void on_usermanagebutton_clicked();
    void on_AddDataButton_clicked();
    void on_AddDataButton_2_clicked();

    // 健康报告与分析
    void on_dashboardButton_clicked();
    void on_trendButton_clicked();
    void on_reportButton_clicked();
    void on_riskButton_clicked();

    // AI 功能
    void on_aiReportButton_clicked();
    void on_aiSettingsButton_clicked();

private:
    Ui::Widget *ui;
    std::unique_ptr<health::HealthManager> manager_;
};
