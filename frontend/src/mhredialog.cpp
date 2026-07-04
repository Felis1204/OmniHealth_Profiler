#include "mhredialog.h"
#include "ui_mhredialog.h"
#include "HealthManager.h"
#include <QMessageBox>
#include <QUuid>
#include <QPushButton>
#include <QDateTime>

MHReDialog::MHReDialog(health::HealthManager *mgr, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MHReDialog)
    , manager_(mgr)
{
    ui->setupUi(this);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("保存");
    ui->dateTimeEdit->setDateTime(QDateTime::currentDateTime());
}

MHReDialog::~MHReDialog()
{
    delete ui;
}

void MHReDialog::accept()
{
    saveData();
}

static health::TimePoint toTimePoint(const QDateTime& dt)
{
    auto ms = dt.toMSecsSinceEpoch();
    return health::TimePoint(std::chrono::milliseconds(ms));
}

static QDateTime fromTimePoint(const health::TimePoint& tp)
{
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  tp.time_since_epoch()).count();
    return QDateTime::fromMSecsSinceEpoch(ms);
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
    record.timestamp  = toTimePoint(ui->dateTimeEdit->dateTime());
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
    ui->dateTimeEdit->setDateTime(fromTimePoint(record.timestamp));
    editingId_ = record.id;
}
