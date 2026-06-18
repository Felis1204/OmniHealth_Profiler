#include "widget.h"
#include "ui_widget.h"
#include "HealthManager.h"
#include "AddData.h"

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
    AddDataDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        // 用户点了 Save / OK
    }
}

