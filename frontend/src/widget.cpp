#include "widget.h"
#include "ui_widget.h"
#include "HealthManager.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    , manager_(CreateHealthManager())
{
    ui->setupUi(this);
}

Widget::~Widget()
{
    delete ui;
}

void Widget::on_pushButton_clicked()
{   QString a;
    a=QString::fromStdString(manager_->aalqp());
    ui->textBrowser->setText(a);
}

