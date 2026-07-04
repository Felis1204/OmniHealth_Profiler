#ifndef REPORTDIALOG_H
#define REPORTDIALOG_H

#include <QDialog>

namespace health { class HealthManager; }

QT_BEGIN_NAMESPACE
namespace Ui { class ReportDialog; }
QT_END_NAMESPACE

class ReportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReportDialog(health::HealthManager *mgr,
                           QWidget *parent = nullptr);
    ~ReportDialog();

private slots:
    void on_generateButton_clicked();
    void on_closeButton_clicked();

private:
    /// @brief 将本地报告纯文本转为带主题色的 HTML
    static QString formatLocalReport(const std::string& raw);

    Ui::ReportDialog *ui;
    health::HealthManager *manager_;
};

#endif // REPORTDIALOG_H
