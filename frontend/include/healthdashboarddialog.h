#ifndef HEALTHDASHBOARDDIALOG_H
#define HEALTHDASHBOARDDIALOG_H

#include <QDialog>

namespace health { class HealthManager; }

QT_BEGIN_NAMESPACE
namespace Ui { class HealthDashboardDialog; }
QT_END_NAMESPACE

class HealthDashboardDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HealthDashboardDialog(health::HealthManager *mgr,
                                    QWidget *parent = nullptr);
    ~HealthDashboardDialog();

private:
    Ui::HealthDashboardDialog *ui;
    health::HealthManager *manager_;
};

#endif // HEALTHDASHBOARDDIALOG_H
