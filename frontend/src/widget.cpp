#include "widget.h"
#include "ui_widget.h"
#include "HealthManager.h"
#include "AddData.h"
#include "datamanage.h"
#include "usermanage.h"
#include "healthdashboarddialog.h"
#include "trenddialog.h"
#include "reportdialog.h"
#include <QMessageBox>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    , manager_(health::createHealthManager())
{
    ui->setupUi(this);
}

Widget::~Widget()
{
    delete ui;
}

void Widget::on_pushButton_clicked()
{
    QString report = QString::fromStdString(manager_->generateHealthReport());
    ui->textBrowser->setText(report);
}


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
// 输出功能（与输入风格一致：按钮 → 弹对话框）
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
    QMessageBox::information(this, "提示", "AI 报告功能即将上线");
}

