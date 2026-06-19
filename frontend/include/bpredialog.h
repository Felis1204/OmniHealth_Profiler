#ifndef BPREDIALOG_H
#define BPREDIALOG_H

#include <QDialog>
#include "Models/BloodPressureRecord.h"

namespace health { class HealthManager; }

namespace Ui {
class BPReDialog;
}

class BPReDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BPReDialog(health::HealthManager *mgr,
                        QWidget *parent = nullptr);
    ~BPReDialog();

    void saveData();

    void loadRecord(const health::BloodPressureRecord& record);

private:
    Ui::BPReDialog *ui;
    health::HealthManager *manager_;
    std::string editingId_;

    void accept() override;
};

#endif // BPREDIALOG_H
