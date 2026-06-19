#ifndef DATAMANAGE_H
#define DATAMANAGE_H

#include <QDialog>
#include <QList>
#include "Models/VitalsRecord.h"
#include "Models/LabTestRecord.h"
#include "Models/BloodPressureRecord.h"
#include "Models/MedicalHistoryRecord.h"

namespace health { class HealthManager; }

namespace Ui {
class DataManage;
}

/// @brief 一条查询结果的通用封装
struct DataRow {
    int typeIndex;          // 0=体征 1=检验 2=血压 3=病历
    std::string id;         // 记录主键
    QString summary;        // 显示摘要
    QString dateStr;        // 日期字符串

    health::VitalsRecord         vitals;
    health::LabTestRecord        labTest;
    health::BloodPressureRecord  bp;
    health::MedicalHistoryRecord mh;
};

class DataManage : public QDialog
{
    Q_OBJECT

public:
    explicit DataManage(health::HealthManager *mgr,
                        QWidget *parent = nullptr);
    ~DataManage();

private slots:
    void on_searchButton_clicked();
    void on_viewButton_clicked();
    void on_deleteButton_clicked();
    void on_closeButton_clicked();

private:
    void searchVitals();
    void searchLabTests();
    void searchBP();
    void searchMH();
    void populateTable();

    Ui::DataManage *ui;
    health::HealthManager *manager_;
    QList<DataRow> results_;
};

#endif // DATAMANAGE_H
