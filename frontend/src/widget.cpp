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
#include "ASCVDCalculator.h"

#include <QMessageBox>
#include <sstream>

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

// ---- 旧版测试按钮（保留兼容）----
void Widget::on_pushButton_clicked()
{
    QString report = QString::fromStdString(manager_->generateHealthReport());
    ui->textBrowser->setText(report);
}

// ---- 个人档案 ----
void Widget::on_AddDataButton_clicked()
{
    AddDataDialog dlg(manager_.get(), this);
    if (dlg.exec() == QDialog::Accepted) {
        // 用户点了 Save / OK
    }
}


void Widget::on_AddDataButton_2_clicked()
{
    DataManage dlg(manager_.get(), this);
    if (dlg.exec() == QDialog::Accepted) {
        // 用户点了 Save / OK
    }
}

void Widget::on_usermanagebutton_clicked()
{
    UserManage dlg(manager_.get(), this);
    dlg.exec();
}

// ============================================================
// 输出功能（远程）
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

void Widget::on_aiButton_clicked()
{
    // 使用新的 AIReportDialog 替代占位提示
    AIReportDialog dlg(manager_.get(), this);
    dlg.exec();
}

// ============================================================
// AI 功能（本地新增）
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
        // 配置已保存，更新按钮提示
        if (manager_->isLLMConfigured()) {
            ui->aiReportButton->setToolTip("AI 顾问已就绪，点击生成 AI 健康报告");
        }
    }
}

void Widget::on_localReportButton_clicked()
{
    std::string report = manager_->generateHealthReport();
    ui->textBrowser->setText(QString::fromStdString(report));
}

void Widget::on_riskButton_clicked()
{
    std::ostringstream oss;

    // BMI
    double bmi = manager_->calculateBMI();
    oss << "【BMI 身体质量指数】\n"
        << "  BMI: " << bmi << "\n"
        << "  分级: " << manager_->getBMICategory() << "\n\n";

    // ASCVD
    double ascvd = manager_->calculateASCVDScore();
    oss << "【ASCVD 10年心血管风险】\n"
        << "  风险值: " << ascvd << "%\n"
        << "  分层: " << health::ASCVDCalculator::getRiskCategory(ascvd) << "\n\n";

    // TyG
    auto tyg = manager_->calculateTyGIndex();
    oss << "【TyG 胰岛素抵抗指数】\n"
        << "  评分: " << tyg.score << "\n"
        << "  评估: " << tyg.riskLevel << "\n\n";

    // CDRS
    auto cdrs = manager_->calculateCDRS();
    oss << "【CDRS 糖尿病风险评分】\n"
        << "  评分: " << static_cast<int>(cdrs.score) << " 分\n"
        << "  评估: " << cdrs.riskLevel << "\n";

    ui->textBrowser->setText(QString::fromStdString(oss.str()));
}
