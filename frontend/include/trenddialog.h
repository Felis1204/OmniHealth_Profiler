#ifndef TRENDDIALOG_H
#define TRENDDIALOG_H

#include <QDialog>

namespace health { class HealthManager; }

QT_BEGIN_NAMESPACE
namespace Ui { class TrendDialog; }
QT_END_NAMESPACE

class TrendDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TrendDialog(health::HealthManager *mgr,
                          QWidget *parent = nullptr);
    ~TrendDialog();

private slots:
    void on_searchButton_clicked();
    void on_closeButton_clicked();

private:
    void clearChart();
    void updateStats(const QString &text);

    Ui::TrendDialog *ui;
    health::HealthManager *manager_;
};

#endif // TRENDDIALOG_H
