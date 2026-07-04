#include "trenddialog.h"
#include "ui_trenddialog.h"
#include "HealthManager.h"

#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QScatterSeries>
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

    // 指标切换按钮
    connect(ui->prevBtn, &QPushButton::clicked,
            this, &TrendDialog::on_prevBtn_clicked);
    connect(ui->nextBtn, &QPushButton::clicked,
            this, &TrendDialog::on_nextBtn_clicked);
}

TrendDialog::~TrendDialog()
{
    delete ui;
}

// ============================================================
// 查询
// ============================================================
void TrendDialog::on_searchButton_clicked()
{
    health::HealthRecordType type;
    switch (ui->typeCombo->currentIndex()) {
    case 0:  type = health::HealthRecordType::VITALS;   break;
    case 1:  type = health::HealthRecordType::LAB_TEST;  break;
    case 2:  type = health::HealthRecordType::BP;        break;
    default: return;
    }

    auto toTimePoint = [](const QDate &d) -> health::TimePoint {
        QDateTime dt(d, QTime(0, 0, 0));
        auto ms = dt.toMSecsSinceEpoch();
        return health::TimePoint(std::chrono::milliseconds(ms));
    };

    report_ = manager_->analyzeTrendReport(
        type,
        toTimePoint(ui->fromDate->date()),
        toTimePoint(ui->toDate->date())
    );

    if (report_.isEmpty()) {
        clearChart();
        ui->statsDisplay->clear();
        ui->metricCombo->clear();
        ui->metricCombo->setEnabled(false);
        ui->prevBtn->setEnabled(false);
        ui->nextBtn->setEnabled(false);
        ui->metricCountLabel->clear();

        auto *layout = new QVBoxLayout(ui->chartContainer);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *label = new QLabel("该时间范围内无数据");
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("font-size: 18px; color: #aaa;");
        layout->addWidget(label);
        return;
    }

    // 填充指标选择器
    ui->metricCombo->blockSignals(true);
    ui->metricCombo->clear();
    for (const auto &m : report_.metrics) {
        ui->metricCombo->addItem(
            QString::fromStdString(m.metricName + " (" + m.unit + ")"));
    }
    ui->metricCombo->blockSignals(false);

    currentMetricIndex_ = 0;
    ui->metricCombo->setCurrentIndex(0);
    updateMetricSelector();
    drawCurrentMetric();
    updateStatsForCurrent();
}

// ============================================================
// 指标切换
// ============================================================
void TrendDialog::on_metricCombo_currentIndexChanged(int index)
{
    if (index < 0 || index >= static_cast<int>(report_.metrics.size()))
        return;
    currentMetricIndex_ = index;
    updateMetricSelector();
    drawCurrentMetric();
    updateStatsForCurrent();
}

void TrendDialog::on_prevBtn_clicked()
{
    if (currentMetricIndex_ > 0) {
        currentMetricIndex_--;
        ui->metricCombo->setCurrentIndex(currentMetricIndex_);
    }
}

void TrendDialog::on_nextBtn_clicked()
{
    if (currentMetricIndex_ < static_cast<int>(report_.metrics.size()) - 1) {
        currentMetricIndex_++;
        ui->metricCombo->setCurrentIndex(currentMetricIndex_);
    }
}

void TrendDialog::updateMetricSelector()
{
    int total = static_cast<int>(report_.metrics.size());
    ui->prevBtn->setEnabled(currentMetricIndex_ > 0);
    ui->nextBtn->setEnabled(currentMetricIndex_ < total - 1);
    ui->metricCountLabel->setText(
        QString("%1 / %2").arg(currentMetricIndex_ + 1).arg(total));
}

// ============================================================
// 绘制当前指标的图表
// ============================================================
void TrendDialog::drawCurrentMetric()
{
    clearChart();

    if (currentMetricIndex_ < 0 ||
        currentMetricIndex_ >= static_cast<int>(report_.metrics.size()))
        return;

    const auto &metric = report_.metrics[currentMetricIndex_];

    auto *chart = new QChart();
    chart->setTitle(QString::fromStdString(metric.metricName));
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->legend()->hide();  // 单指标不需要图例

    // 折线 + 数据点（不显示数值标签）
    auto *series = new QLineSeries();
    series->setName(QString::fromStdString(metric.metricName));
    series->setPointsVisible(true);
    series->setPen(QPen(QColor("#1565c0"), 2));

    double minVal = 1e18, maxVal = -1e18;
    qint64 minTime = 0, maxTime = 0;

    for (const auto &pt : metric.dataPoints) {
        QDateTime dt = QDateTime::fromString(
            QString::fromStdString(pt.timestamp), Qt::ISODate);
        if (!dt.isValid()) continue;

        qint64 ms = dt.toMSecsSinceEpoch();
        series->append(ms, pt.value);

        if (pt.value < minVal) minVal = pt.value;
        if (pt.value > maxVal) maxVal = pt.value;
        if (minTime == 0 || ms < minTime) minTime = ms;
        if (ms > maxTime) maxTime = ms;
    }

    chart->addSeries(series);

    // X 轴（时间）
    auto *axisX = new QDateTimeAxis();
    axisX->setFormat("MM-dd");
    axisX->setTitleText("日期");
    axisX->setTickCount(std::min(static_cast<int>(metric.dataPoints.size()), 10));
    if (minTime > 0 && maxTime > minTime) {
        axisX->setRange(
            QDateTime::fromMSecsSinceEpoch(minTime),
            QDateTime::fromMSecsSinceEpoch(maxTime));
    }
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    // Y 轴（数值）— 单指标，精确定制范围
    auto *axisY = new QValueAxis();
    axisY->setTitleText(QString::fromStdString(metric.unit));
    if (maxVal > minVal) {
        double margin = (maxVal - minVal) * 0.15;
        if (margin < 1.0) margin = 1.0;
        axisY->setRange(minVal - margin, maxVal + margin);
        axisY->setTickCount(6);
        axisY->setLabelFormat("%.4g");  // 智能精度，避免长小数
    }
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    // 显示图表
    chart->setMargins(QMargins(8, 12, 12, 12));
    chart->setPlotAreaBackgroundVisible(true);
    chart->setBackgroundRoundness(6);
    auto *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setContentsMargins(0, 0, 0, 0);
    auto *layout = new QVBoxLayout(ui->chartContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(chartView);
}

// ============================================================
// 统计摘要
// ============================================================
void TrendDialog::updateStatsForCurrent()
{
    if (currentMetricIndex_ < 0 ||
        currentMetricIndex_ >= static_cast<int>(report_.metrics.size())) {
        ui->statsDisplay->clear();
        return;
    }

    const auto &m = report_.metrics[currentMetricIndex_];

    // 用 HTML 表格展示
    QString slopeStr = QString::number(m.slope, 'f', 2);
    QString trendIcon = "➡️ 平稳";
    if (m.slope > 0.5)       trendIcon = "📈 上升";
    else if (m.slope < -0.5) trendIcon = "📉 下降";

    QString html = "<table width='100%' cellspacing='0' cellpadding='2'>"
                   "<tr style='background:#f0f0f0; font-weight:bold;'>"
                   "<td>数据点</td><td>均值</td><td>最小值</td>"
                   "<td>最大值</td><td>中位数</td><td>斜率</td><td>趋势</td></tr>"
                   "<tr>"
                   "<td>" + QString::number(m.count) + "</td>"
                   "<td><b>" + QString::number(m.average, 'f', 1) + "</b></td>"
                   "<td>" + QString::number(m.min, 'f', 1) + "</td>"
                   "<td>" + QString::number(m.max, 'f', 1) + "</td>"
                   "<td>" + QString::number(m.median, 'f', 1) + "</td>"
                   "<td>" + slopeStr + "</td>"
                   "<td>" + trendIcon + "</td>"
                   "</tr></table>";

    ui->statsDisplay->setHtml(html);
}

// ============================================================
// 清理旧图表
// ============================================================
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

// ============================================================
// 关闭
// ============================================================
void TrendDialog::on_closeButton_clicked()
{
    close();
}
