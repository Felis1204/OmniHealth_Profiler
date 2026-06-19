#include "widget.h"
#include "ui_widget.h"
#include "HealthManager.h"
#include "AddData.h"
#include "datamanage.h"
#include "usermanage.h"

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

