#ifndef LABTESTREDIALOG_H
#define LABTESTREDIALOG_H

#include <QDialog>
#include "Models/LabTestRecord.h"

namespace health { class HealthManager; }

namespace Ui {
class LabTestReDialog;
}

class LabTestReDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LabTestReDialog(health::HealthManager *mgr,
                             QWidget *parent = nullptr);
    ~LabTestReDialog();

    void saveData();

    void loadRecord(const health::LabTestRecord& record);

private:
    Ui::LabTestReDialog *ui;
    health::HealthManager *manager_;
    std::string editingId_;

    void accept() override;
};

#endif // LABTESTREDIALOG_H
