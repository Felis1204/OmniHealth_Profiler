#include "labtestredialog.h"
#include "ui_labtestredialog.h"
#include "HealthManager.h"
#include <QMessageBox>
#include <QUuid>
#include <QPushButton>
#include <QDateTime>

LabTestReDialog::LabTestReDialog(health::HealthManager *mgr, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LabTestReDialog)
    , manager_(mgr)
{
    ui->setupUi(this);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("保存");
    ui->dateTimeEdit->setDateTime(QDateTime::currentDateTime());
}

LabTestReDialog::~LabTestReDialog()
{
    delete ui;
}

void LabTestReDialog::accept()
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

void LabTestReDialog::saveData()
{
    double glucose       = ui->fastingGlucoseSpinBox->value();
    double cholesterol   = ui->totalCholesterolSpinBox->value();
    double ldl           = ui->ldlSpinBox->value();
    double hdl           = ui->hdlSpinBox->value();
    double triglycerides = ui->triglyceridesSpinBox->value();
    double uricAcid      = ui->uricAcidSpinBox->value();

    if (glucose <= 0 && cholesterol <= 0 && ldl <= 0 && hdl <= 0
        && triglycerides <= 0 && uricAcid <= 0) {
        QMessageBox::warning(this, "提示", "请至少填写一项数据");
        return;
    }

    health::LabTestRecord record;
    record.recordType = health::HealthRecordType::LAB_TEST;
    record.timestamp  = toTimePoint(ui->dateTimeEdit->dateTime());

    if (glucose > 0)       record.fastingGlucose     = glucose;
    if (cholesterol > 0)   record.totalCholesterol   = cholesterol;
    if (ldl > 0)           record.ldlC               = ldl;
    if (hdl > 0)           record.hdlC               = hdl;
    if (triglycerides > 0) record.triglycerides      = triglycerides;
    if (uricAcid > 0)      record.uricAcid           = uricAcid;

    if (manager_) {
        if (editingId_.empty()) {
            record.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            manager_->addLabTestRecord(record);
        } else {
            record.id = editingId_;
            manager_->updateLabTestRecord(record);
        }
    }

    QDialog::accept();
}

void LabTestReDialog::loadRecord(const health::LabTestRecord& record)
{
    if (record.fastingGlucose)     ui->fastingGlucoseSpinBox->setValue(*record.fastingGlucose);
    if (record.totalCholesterol)   ui->totalCholesterolSpinBox->setValue(*record.totalCholesterol);
    if (record.ldlC)               ui->ldlSpinBox->setValue(*record.ldlC);
    if (record.hdlC)               ui->hdlSpinBox->setValue(*record.hdlC);
    if (record.triglycerides)      ui->triglyceridesSpinBox->setValue(*record.triglycerides);
    if (record.uricAcid)           ui->uricAcidSpinBox->setValue(*record.uricAcid);
    ui->dateTimeEdit->setDateTime(fromTimePoint(record.timestamp));
    editingId_ = record.id;
}
