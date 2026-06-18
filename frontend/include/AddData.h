#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class AddDataDialog; }
QT_END_NAMESPACE

class AddDataDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddDataDialog(QWidget *parent = nullptr);
    ~AddDataDialog() override;
private:
    Ui::AddDataDialog *ui;
};
