#include "bpredialog.h"
#include "ui_bpredialog.h"
#include "HealthManager.h"
#include <QMessageBox>
#include <QUuid>
#include <QPushButton>
#include <QDateTime>

BPReDialog::BPReDialog(health::HealthManager *mgr, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::BPReDialog)
    , manager_(mgr)
{
    ui->setupUi(this);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("保存");
    ui->dateTimeEdit->setDateTime(QDateTime::currentDateTime());
}

BPReDialog::~BPReDialog()
{
    delete ui;
}

void BPReDialog::accept()
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

void BPReDialog::saveData()
{
    int systolic  = ui->systolicSpinBox->value();
    int diastolic = ui->diastolicSpinBox->value();

    if (systolic <= 0 && diastolic <= 0) {
        QMessageBox::warning(this, "提示", "请至少填写一项数据");
        return;
    }

    health::BloodPressureRecord record;
    record.recordType = health::HealthRecordType::BP;
    record.timestamp  = toTimePoint(ui->dateTimeEdit->dateTime());

    if (systolic > 0)  record.systolic  = systolic;
    if (diastolic > 0) record.diastolic = diastolic;

    if (manager_) {
        if (editingId_.empty()) {
            record.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            manager_->addBloodPressureRecord(record);
        } else {
            record.id = editingId_;
            manager_->updateBloodPressureRecord(record);
        }
    }

    QDialog::accept();
}

void BPReDialog::loadRecord(const health::BloodPressureRecord& record)
{
    if (record.systolic)  ui->systolicSpinBox->setValue(*record.systolic);
    if (record.diastolic) ui->diastolicSpinBox->setValue(*record.diastolic);
    ui->dateTimeEdit->setDateTime(fromTimePoint(record.timestamp));
    editingId_ = record.id;
}
