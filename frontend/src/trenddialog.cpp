#include "trenddialog.h"
#include "ui_trenddialog.h"
#include "HealthManager.h"

#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QDateTimeAxis>
#include <QValueAxis>
#include <QVBoxLayout>
#include <QLabel>

TrendDialog::TrendDialog(health::HealthManager *mgr,
                           QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TrendDialog)
    , manager_(mgr)
{
    ui->setupUi(this);

    // 默认日期范围：最近 30 天
    ui->fromDate->setDate(QDate::currentDate().addMonths(-1));
    ui->toDate->setDate(QDate::currentDate());
}

TrendDialog::~TrendDialog()
{
    delete ui;
}

void TrendDialog::on_searchButton_clicked()
{
    // 确定记录类型
    health::HealthRecordType type;
    switch (ui->typeCombo->currentIndex()) {
    case 0:  type = health::HealthRecordType::VITALS;   break;
    case 1:  type = health::HealthRecordType::LAB_TEST;  break;
    case 2:  type = health::HealthRecordType::BP;        break;
    default: return;
    }

    //QDate 转 TimePoint
    auto toTimePoint = [](const QDate &d) -> health::TimePoint {
        QDateTime dt(d, QTime(0, 0, 0));
        auto ms = dt.toMSecsSinceEpoch();
        return health::TimePoint(std::chrono::milliseconds(ms));
    };

    // 调用后端趋势分析接口
    auto report = manager_->analyzeTrendReport(
        type,
        toTimePoint(ui->fromDate->date()),
        toTimePoint(ui->toDate->date())
    );

    // 清空旧图表
    clearChart();

    // 无数据
    if (report.isEmpty()) {
        auto *label = new QLabel("该时间段内无数据");
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("font-size: 18px; color: #888;");
        auto *layout = new QVBoxLayout(ui->chartContainer);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(label);
        updateStats("无数据");
        return;
    }

    //创建图表
    auto *chart = new QChart();
    chart->setTitle(QString::fromStdString(report.title));
    chart->setAnimationOptions(QChart::SeriesAnimations);

    //每条指标画一条折线
    double globalMin = 1e18, globalMax = -1e18;
    for (const auto &metric : report.metrics) {
        auto *series = new QLineSeries();
        series->setName(QString::fromStdString(metric.metricName));
        series->setPointsVisible(true);

        for (const auto &pt : metric.dataPoints) {
            QDateTime dt = QDateTime::fromString(
                QString::fromStdString(pt.timestamp), Qt::ISODate);
            if (!dt.isValid()) continue;
            series->append(dt.toMSecsSinceEpoch(), pt.value);
        }

        chart->addSeries(series);

        if (metric.min < globalMin) globalMin = metric.min;
        if (metric.max > globalMax) globalMax = metric.max;
    }

    //创建 X 轴（时间）
    auto *axisX = new QDateTimeAxis();
    axisX->setFormat("MM-dd");
    axisX->setTitleText("日期");
    chart->addAxis(axisX, Qt::AlignBottom);

    //创建 Y 轴（数值）
    auto *axisY = new QValueAxis();
    axisY->setTitleText("数值");
    if (globalMax > globalMin) {
        double margin = (globalMax - globalMin) * 0.1;
        if (margin == 0) margin = 1.0;
        axisY->setRange(globalMin - margin, globalMax + margin);
    }
    chart->addAxis(axisY, Qt::AlignLeft);

    //把坐标轴附加到每条折线
    for (auto *s : chart->series()) {
        s->attachAxis(axisX);
        s->attachAxis(axisY);
    }

    //显示图表
    auto *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    auto *layout = new QVBoxLayout(ui->chartContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(chartView);

    //更新底部统计信息
    QString stats;
    for (const auto &m : report.metrics) {
        QString trendIcon = "平稳➡️";
        if (m.slope > 0.5)      trendIcon = "上升📈";
        else if (m.slope < -0.5) trendIcon = "下降📉";

        stats += QString("%1 均值 %2 | 范围 %3~%4 | 趋势 %5  \n")
            .arg(QString::fromStdString(m.metricName))
            .arg(m.average, 0, 'f', 1)
            .arg(m.min, 0, 'f', 1)
            .arg(m.max, 0, 'f', 1)
            .arg(trendIcon);
    }
    updateStats(stats);
}

void TrendDialog::on_closeButton_clicked()
{
    close();
}

void TrendDialog::clearChart()
{
    QLayout *oldLayout = ui->chartContainer->layout();
    if (oldLayout) {
        QLayoutItem *item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete oldLayout;
    }
}

void TrendDialog::updateStats(const QString &text)
{
    ui->statsDisplay->setPlainText(text);
}
