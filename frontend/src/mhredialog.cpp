#include "mhredialog.h"
#include "ui_mhredialog.h"
#include "HealthManager.h"
#include <QMessageBox>
#include <QUuid>
#include <QPushButton>

MHReDialog::MHReDialog(health::HealthManager *mgr, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MHReDialog)
    , manager_(mgr)
{
    ui->setupUi(this);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("保存");
}

MHReDialog::~MHReDialog()
{
    delete ui;
}

void MHReDialog::accept()
{
    saveData();
}

void MHReDialog::saveData()
{
    QString content = ui->contentEdit->toPlainText().trimmed();

    if (content.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入病历内容");
        return;
    }

    health::MedicalHistoryRecord record;
    record.recordType = health::HealthRecordType::HISTORY;
    record.timestamp  = std::chrono::system_clock::now();

    record.category = ui->categoryCombo->currentText().toStdString();
    record.content  = content.toStdString();

    if (manager_) {
        if (editingId_.empty()) {
            record.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            manager_->addMedicalHistoryRecord(record);
        } else {
            record.id = editingId_;
            manager_->updateMedicalHistoryRecord(record);
        }
    }

    QDialog::accept();
}

void MHReDialog::loadRecord(const health::MedicalHistoryRecord& record)
{
    ui->categoryCombo->setCurrentText(QString::fromStdString(record.category));
    ui->contentEdit->setPlainText(QString::fromStdString(record.content));
    editingId_ = record.id;
}
