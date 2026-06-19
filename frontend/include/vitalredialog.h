#ifndef VITALREDIALOG_H
#define VITALREDIALOG_H

#include <QDialog>
#include "Models/VitalsRecord.h"

namespace health { class HealthManager; }

namespace Ui {
class VitalReDialog;
}

class VitalReDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VitalReDialog(health::HealthManager *mgr,
                           QWidget *parent = nullptr);
    ~VitalReDialog();

    void saveData();

    /// @brief 加载已有记录到界面（编辑模式）
    void loadRecord(const health::VitalsRecord& record);

private:
    Ui::VitalReDialog *ui;
    health::HealthManager *manager_;
    std::string editingId_;   // 空=新增，非空=编辑已有记录

    void accept() override;
};

#endif // VITALREDIALOG_H
