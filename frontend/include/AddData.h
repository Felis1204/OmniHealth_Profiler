#pragma once

#include <QDialog>

namespace health { class HealthManager; }

QT_BEGIN_NAMESPACE
namespace Ui { class AddDataDialog; }
QT_END_NAMESPACE

class AddDataDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddDataDialog(health::HealthManager *mgr,
                           QWidget *parent = nullptr);
    ~AddDataDialog() override;

private slots:
    void on_VitalRecord_clicked();
    void on_LTRecord_clicked();
    void on_BPRecord_clicked();
    void on_MHRecord_clicked();

    void on_OKbutton_clicked();

private:
    Ui::AddDataDialog *ui;
    health::HealthManager *manager_;
};
