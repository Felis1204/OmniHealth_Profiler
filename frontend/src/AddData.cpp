#include "AddData.h"
#include "ui_AddData.h"
#include "vitalredialog.h"
#include "labtestredialog.h"
#include "bpredialog.h"
#include "mhredialog.h"


AddDataDialog::AddDataDialog(health::HealthManager *mgr, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddDataDialog)
    , manager_(mgr)
{
    ui->setupUi(this);
}

AddDataDialog::~AddDataDialog() { delete ui; }
void AddDataDialog::on_VitalRecord_clicked()
{
    VitalReDialog dlg(manager_, this);
    dlg.exec();
}


void AddDataDialog::on_LTRecord_clicked()
{
    LabTestReDialog dlg(manager_, this);
    dlg.exec();
}


void AddDataDialog::on_BPRecord_clicked()
{
    BPReDialog dlg(manager_, this);
    dlg.exec();
}


void AddDataDialog::on_MHRecord_clicked()
{
    MHReDialog dlg(manager_, this);
    dlg.exec();
}


void AddDataDialog::on_OKbutton_clicked()
{
    QDialog::accept();
}

