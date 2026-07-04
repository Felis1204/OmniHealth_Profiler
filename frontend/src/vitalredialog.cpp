#include "vitalredialog.h"
#include "ui_vitalredialog.h"
#include "HealthManager.h"
#include <QMessageBox>
#include <QUuid>
#include <QPushButton>
#include <QDateTime>

VitalReDialog::VitalReDialog(health::HealthManager *mgr, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::VitalReDialog)
    , manager_(mgr)
{
    ui->setupUi(this);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("保存");
    ui->dateTimeEdit->setDateTime(QDateTime::currentDateTime());
}

VitalReDialog::~VitalReDialog()
{
    delete ui;
}

void VitalReDialog::accept()
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

void VitalReDialog::saveData()
{
    health::VitalsRecord record;

    double height = ui->hightSpinBox->value();
    double weight = ui->weightSpinBox->value();
    double waist  = ui->waistSpinBox_2->value();
    double hr     = ui->heartrateSpinBox_3->value();
    int    steps  = ui->stepspinBox->value();
    double sleep  = ui->sleephSpinBox_4->value();

    if (height <= 0 && weight <= 0 && waist <= 0 && hr <= 0
        && steps <= 0 && sleep <= 0) {
        QMessageBox::warning(this, "提示", "请至少填写一项数据");
        return;
    }

    record.recordType = health::HealthRecordType::VITALS;
    record.timestamp = toTimePoint(ui->dateTimeEdit->dateTime());

    if (height > 0) record.heightCm  = height;
    if (weight > 0) record.weightKg  = weight;
    if (waist  > 0) record.waistCm   = waist;
    if (hr     > 0) record.heartRate  = hr;
    if (steps  > 0) record.steps      = steps;
    if (sleep  > 0) record.sleepHours = sleep;

    if (manager_) {
        if (editingId_.empty()) {
            record.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            manager_->addVitalsRecord(record);
        } else {
            record.id = editingId_;
            manager_->updateVitalsRecord(record);
        }
    }

    QDialog::accept();
}

void VitalReDialog::loadRecord(const health::VitalsRecord& record)
{
    if (record.heightCm)  ui->hightSpinBox->setValue(*record.heightCm);
    if (record.weightKg)  ui->weightSpinBox->setValue(*record.weightKg);
    if (record.waistCm)   ui->waistSpinBox_2->setValue(*record.waistCm);
    if (record.heartRate) ui->heartrateSpinBox_3->setValue(*record.heartRate);
    if (record.steps)     ui->stepspinBox->setValue(*record.steps);
    if (record.sleepHours) ui->sleephSpinBox_4->setValue(*record.sleepHours);
    ui->dateTimeEdit->setDateTime(fromTimePoint(record.timestamp));
    editingId_ = record.id;
}
