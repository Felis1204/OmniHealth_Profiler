#include "AddData.h"
#include "ui_AddData.h"       // AUTOUIC 生成的

AddDataDialog::AddDataDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddDataDialog)
{
    ui->setupUi(this);
}

AddDataDialog::~AddDataDialog() { delete ui; }