#include "widget.h"
#include "ui_widget.h"
#include "HealthManager.h"
#include "AddData.h"
#include "datamanage.h"
#include "usermanage.h"
#include "healthdashboarddialog.h"
#include "trenddialog.h"
#include "reportdialog.h"
#include "AISettingsDialog.h"
#include "AIReportDialog.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    , manager_(health::createHealthManager())
{
    ui->setupUi(this);

    // 启动时检查 AI 状态并更新按钮提示
    if (manager_->isLLMConfigured()) {
        ui->aiReportButton->setToolTip("AI 顾问已就绪，点击生成 AI 健康报告");
    } else {
        ui->aiReportButton->setToolTip(
            "AI 顾问未配置，将降级为本地报告。\n请先在 AI 设置中配置 API Key。");
    }
}

Widget::~Widget()
{
    delete ui;
}

// ============================================================
// 个人档案
// ============================================================

void Widget::on_usermanagebutton_clicked()
{
    UserManage dlg(manager_.get(), this);
    dlg.exec();
}

void Widget::on_AddDataButton_clicked()
{
    AddDataDialog dlg(manager_.get(), this);
    dlg.exec();
}

void Widget::on_AddDataButton_2_clicked()
{
    DataManage dlg(manager_.get(), this);
    dlg.exec();
}

// ============================================================
// 健康报告与分析
// ============================================================

void Widget::on_dashboardButton_clicked()
{
    HealthDashboardDialog dlg(manager_.get(), this);
    dlg.exec();
}

void Widget::on_trendButton_clicked()
{
    TrendDialog dlg(manager_.get(), this);
    dlg.exec();
}

void Widget::on_reportButton_clicked()
{
    ReportDialog dlg(manager_.get(), this);
    dlg.exec();
}

void Widget::on_riskButton_clicked()
{
    // 风险总览 → 复用仪表盘（已包含四项评估）
    HealthDashboardDialog dlg(manager_.get(), this);
    dlg.setWindowTitle("风险总览");
    dlg.exec();
}

// ============================================================
// AI 功能
// ============================================================

void Widget::on_aiReportButton_clicked()
{
    AIReportDialog dlg(manager_.get(), this);
    dlg.exec();
}

void Widget::on_aiSettingsButton_clicked()
{
    AISettingsDialog dlg(manager_.get(), this);
    if (dlg.exec() == QDialog::Accepted) {
        if (manager_->isLLMConfigured()) {
            ui->aiReportButton->setToolTip("AI 顾问已就绪，点击生成 AI 健康报告");
        }
    }
}
