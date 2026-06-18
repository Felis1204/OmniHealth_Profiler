#include "widget.h"
#include "ui_widget.h"
#include "HealthManager.h"

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

