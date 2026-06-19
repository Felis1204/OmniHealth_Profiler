#include "datamanage.h"
#include "ui_datamanage.h"
#include "vitalredialog.h"
#include "labtestredialog.h"
#include "bpredialog.h"
#include "mhredialog.h"
#include "HealthManager.h"
#include <QMessageBox>
#include <QHeaderView>

// ============================================================
// 工具函数：QDate ↔ TimePoint
// ============================================================
static const char* kTypeNames[] = {
    "体征", "临床检验", "血压", "病历摘要"
};
static health::TimePoint dateToTimePoint(const QDate& date) {
    QDateTime dt(date, QTime(0, 0, 0));
    auto ms = dt.toMSecsSinceEpoch();
    return health::TimePoint(std::chrono::milliseconds(ms));
}

static QString timePointToStr(const health::TimePoint& tp) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  tp.time_since_epoch()).count();
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms);
    return dt.toString("yyyy-MM-dd hh:mm");
}

// ============================================================
// 构造 / 析构
// ============================================================
DataManage::DataManage(health::HealthManager *mgr, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DataManage)
    , manager_(mgr)
{
    ui->setupUi(this);

    // 表格表头自适应
    ui->resultTable->setColumnWidth(0, 140);  // 日期
    ui->resultTable->setColumnWidth(1, 80);   // 类型
    ui->resultTable->horizontalHeader()->setStretchLastSection(true);

    // 切换类型时更新日期框可用状态
    connect(ui->typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        // idx=4 病历摘要 → 禁用日期，其他启用
        bool enable = (idx != 4);
        ui->fromDateEdit->setEnabled(enable);
        ui->toDateEdit->setEnabled(enable);
        ui->label->setEnabled(enable);    // "起始："
        ui->label_2->setEnabled(enable);  // "截止："
    });
}

DataManage::~DataManage()
{
    delete ui;
}

// ============================================================
// 查找
// ============================================================
void DataManage::on_searchButton_clicked()
{
    results_.clear();

    switch (ui->typeCombo->currentIndex()) {
    case 0: searchVitals(); searchLabTests(); searchBP(); searchMH(); break;
    case 1: searchVitals();    break;
    case 2: searchLabTests();  break;
    case 3: searchBP();        break;
    case 4: searchMH();        break;
    }
    populateTable();
}

void DataManage::searchVitals()
{
    if (!manager_) return;
    auto from = dateToTimePoint(ui->fromDateEdit->date());
    auto to   = dateToTimePoint(ui->toDateEdit->date());
    auto records = manager_->getVitalsRecords(from, to);

    for (const auto& r : records) {
        DataRow row;
        row.typeIndex = 0;
        row.id    = r.id;
        row.dateStr = timePointToStr(r.timestamp);
        row.vitals  = r;

        // 构建摘要
        QStringList parts;
        if (r.heightCm)  parts << QString("身高%1cm").arg(*r.heightCm);
        if (r.weightKg)  parts << QString("体重%1kg").arg(*r.weightKg);
        if (r.waistCm)   parts << QString("腰围%1cm").arg(*r.waistCm);
        if (r.heartRate) parts << QString("心率%1bpm").arg(*r.heartRate);
        if (r.steps)     parts << QString("步数%1").arg(*r.steps);
        if (r.sleepHours) parts << QString("睡眠%1h").arg(*r.sleepHours);
        row.summary = parts.join(" | ");

        results_.append(row);
    }
}

void DataManage::searchLabTests()
{
    if (!manager_) return;
    auto from = dateToTimePoint(ui->fromDateEdit->date());
    auto to   = dateToTimePoint(ui->toDateEdit->date());
    auto records = manager_->getLabTestRecords(from, to);

    for (const auto& r : records) {
        DataRow row;
        row.typeIndex = 1;
        row.id     = r.id;
        row.dateStr = timePointToStr(r.timestamp);
        row.labTest = r;

        QStringList parts;
        if (r.fastingGlucose)     parts << QString("血糖%1").arg(*r.fastingGlucose);
        if (r.totalCholesterol)   parts << QString("TC%1").arg(*r.totalCholesterol);
        if (r.ldlC)               parts << QString("LDL%1").arg(*r.ldlC);
        if (r.hdlC)               parts << QString("HDL%1").arg(*r.hdlC);
        if (r.triglycerides)      parts << QString("TG%1").arg(*r.triglycerides);
        if (r.uricAcid)           parts << QString("尿酸%1").arg(*r.uricAcid);
        row.summary = parts.join(" | ");

        results_.append(row);
    }
}

void DataManage::searchBP()
{
    if (!manager_) return;
    auto from = dateToTimePoint(ui->fromDateEdit->date());
    auto to   = dateToTimePoint(ui->toDateEdit->date());
    auto records = manager_->getBloodPressureRecords(from, to);

    for (const auto& r : records) {
        DataRow row;
        row.typeIndex = 2;
        row.id     = r.id;
        row.dateStr = timePointToStr(r.timestamp);
        row.bp     = r;

        if (r.systolic && r.diastolic)
            row.summary = QString("血压 %1/%2 mmHg").arg(*r.systolic).arg(*r.diastolic);
        else if (r.systolic)
            row.summary = QString("收缩压 %1 mmHg").arg(*r.systolic);
        else if (r.diastolic)
            row.summary = QString("舒张压 %1 mmHg").arg(*r.diastolic);

        results_.append(row);
    }
}

void DataManage::searchMH()
{
    if (!manager_) return;
    auto records = manager_->getMedicalHistoryRecords();

    for (const auto& r : records) {
        DataRow row;
        row.typeIndex = 3;
        row.id     = r.id;
        row.dateStr = timePointToStr(r.timestamp);
        row.mh     = r;

        // 摘要取内容前 30 字
        QString content = QString::fromStdString(r.content);
        if (content.length() > 30)
            content = content.left(30) + "...";
        row.summary = QString("[%1] %2")
                          .arg(QString::fromStdString(r.category))
                          .arg(content);

        results_.append(row);
    }
}

void DataManage::populateTable()
{
    ui->resultTable->setRowCount(results_.size());
    for (int i = 0; i < results_.size(); ++i) {
        const auto& row = results_[i];
        ui->resultTable->setItem(i, 0, new QTableWidgetItem(row.dateStr));
        ui->resultTable->setItem(i, 1, new QTableWidgetItem(
            QString::fromUtf8(kTypeNames[row.typeIndex])));
        ui->resultTable->setItem(i, 2, new QTableWidgetItem(row.summary));
    }
}

// ============================================================
// 查看/编辑
// ============================================================
void DataManage::on_viewButton_clicked()
{
    int row = ui->resultTable->currentRow();
    if (row < 0 || row >= results_.size()) {
        QMessageBox::information(this, "提示", "请先选择一条记录");
        return;
    }

    const auto& data = results_[row];

    switch (data.typeIndex) {
    case 0: {
        VitalReDialog dlg(manager_, this);
        dlg.setWindowTitle("编辑体征记录");
        dlg.loadRecord(data.vitals);
        dlg.exec();
        break;
    }
    case 1: {
        LabTestReDialog dlg(manager_, this);
        dlg.setWindowTitle("编辑检验记录");
        dlg.loadRecord(data.labTest);
        dlg.exec();
        break;
    }
    case 2: {
        BPReDialog dlg(manager_, this);
        dlg.setWindowTitle("编辑血压记录");
        dlg.loadRecord(data.bp);
        dlg.exec();
        break;
    }
    case 3: {
        MHReDialog dlg(manager_, this);
        dlg.setWindowTitle("编辑病历摘要");
        dlg.loadRecord(data.mh);
        dlg.exec();
        break;
    }
    }
    // 编辑后刷新
    on_searchButton_clicked();
}

// ============================================================
// 删除
// ============================================================
void DataManage::on_deleteButton_clicked()
{
    int row = ui->resultTable->currentRow();
    if (row < 0 || row >= results_.size()) {
        QMessageBox::information(this, "提示", "请先选择一条记录");
        return;
    }

    auto btn = QMessageBox::question(this, "确认删除",
                 "确定要删除这条记录吗？此操作不可撤销。");
    if (btn != QMessageBox::Yes) return;

    const auto& data = results_[row];
    bool ok = false;

    switch (data.typeIndex) {
    case 0: ok = manager_->deleteVitalsRecord(data.id); break;
    case 1: ok = manager_->deleteLabTestRecord(data.id); break;
    case 2: ok = manager_->deleteBloodPressureRecord(data.id); break;
    case 3: ok = manager_->deleteMedicalHistoryRecord(data.id); break;
    }

    if (ok) {
        QMessageBox::information(this, "成功", "记录已删除");
        on_searchButton_clicked();  // 刷新列表
    } else {
        QMessageBox::warning(this, "失败", "删除失败，请重试");
    }
}

// ============================================================
// 关闭
// ============================================================
void DataManage::on_closeButton_clicked()
{
    close();
}
