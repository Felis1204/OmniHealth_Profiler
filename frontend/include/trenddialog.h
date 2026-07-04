#ifndef TRENDDIALOG_H
#define TRENDDIALOG_H

#include <QDialog>
#include "HealthManager.h"

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
    void on_metricCombo_currentIndexChanged(int index);
    void on_prevBtn_clicked();
    void on_nextBtn_clicked();
    void on_closeButton_clicked();

private:
    void clearChart();
    void drawCurrentMetric();
    void updateStatsForCurrent();
    void updateMetricSelector();

    Ui::TrendDialog *ui;
    health::HealthManager *manager_;
    health::TrendReport report_;
    int currentMetricIndex_ = 0;
};

#endif // TRENDDIALOG_H
