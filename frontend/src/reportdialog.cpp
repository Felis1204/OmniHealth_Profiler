#include "reportdialog.h"
#include "ui_reportdialog.h"
#include "HealthManager.h"

ReportDialog::ReportDialog(health::HealthManager *mgr,
                            QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ReportDialog)
    , manager_(mgr)
{
    ui->setupUi(this);
}

ReportDialog::~ReportDialog()
{
    delete ui;
}

void ReportDialog::on_generateButton_clicked()
{
    // 根据下拉框选择周报或月报
    health::HealthManager::ReportPeriod period;
    if (ui->reportTypeCombo->currentIndex() == 0)
        period = health::HealthManager::ReportPeriod::WEEKLY;
    else
        period = health::HealthManager::ReportPeriod::MONTHLY;

    // 调用后端生成报告
    std::string report = manager_->generateHealthReport(period);

    // 显示在文本框中
    ui->reportDisplay->setPlainText(QString::fromStdString(report));
}

void ReportDialog::on_closeButton_clicked()
{
    close();
}
