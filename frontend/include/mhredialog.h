#ifndef MHREDIALOG_H
#define MHREDIALOG_H

#include <QDialog>
#include "Models/MedicalHistoryRecord.h"

namespace health { class HealthManager; }

namespace Ui {
class MHReDialog;
}

class MHReDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MHReDialog(health::HealthManager *mgr,
                        QWidget *parent = nullptr);
    ~MHReDialog();

    void saveData();

    void loadRecord(const health::MedicalHistoryRecord& record);

private:
    Ui::MHReDialog *ui;
    health::HealthManager *manager_;
    std::string editingId_;

    void accept() override;
};

#endif // MHREDIALOG_H
