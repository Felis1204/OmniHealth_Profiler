#include "reportdialog.h"
#include "ui_reportdialog.h"
#include "HealthManager.h"
#include "Theme.h"

#include <QRegularExpression>

ReportDialog::ReportDialog(health::HealthManager *mgr,
                            QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ReportDialog)
    , manager_(mgr)
{
    ui->setupUi(this);

    // 应用统一主题
    setStyleSheet(Theme::DialogBase());
    ui->reportDisplay->setStyleSheet(
        "QTextBrowser { background: #fff; border: 1px solid #ddd; "
        "border-radius: 6px; padding: 12px; font-size: 13px; color: #333; }");
}

ReportDialog::~ReportDialog()
{
    delete ui;
}

void ReportDialog::on_generateButton_clicked()
{
    health::HealthManager::ReportPeriod period;
    if (ui->reportTypeCombo->currentIndex() == 0)
        period = health::HealthManager::ReportPeriod::WEEKLY;
    else
        period = health::HealthManager::ReportPeriod::MONTHLY;

    std::string report = manager_->generateHealthReport(period);
    QString html = formatLocalReport(report);
    ui->reportDisplay->setHtml(html);
}

void ReportDialog::on_closeButton_clicked()
{
    close();
}

// ============================================================
// formatLocalReport — 将本地报告纯文本转为带主题色的 HTML
// ============================================================
QString ReportDialog::formatLocalReport(const std::string& raw)
{
    QString text = QString::fromStdString(raw).trimmed();
    if (text.isEmpty()) return {};

    auto esc = [](const QString& s) {
        return s.toHtmlEscaped();
    };

    // 全局样式容器
    QString html;
    html += "<div style='font-family: -apple-system, \"Microsoft YaHei\", sans-serif;'>";

    QStringList lines = text.split('\n');
    bool inSection = false;
    QString currentSection;

    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            if (inSection) {
                html += "</div>";  // 关闭上一个 section 卡片
                inSection = false;
                currentSection.clear();
            }
            continue;
        }

        // ---- 标题行（===== 包裹）----
        if (trimmed.startsWith("======") || trimmed.startsWith("======")) {
            // 下一行才是真正的标题
            continue;
        }

        // ---- 章节头：【xxx】 ----
        QRegularExpression sectionRe(R"(^【(.+?)】)");
        QRegularExpressionMatch sm = sectionRe.match(trimmed);
        if (sm.hasMatch()) {
            // 先关闭上一个卡片
            if (inSection) {
                html += "</div>";
                inSection = false;
            }

            QString title = sm.captured(1).trimmed();
            currentSection = title;

            // 根据章节选择卡片颜色
            const char* bg = Theme::CardInfo();
            const char* border = Theme::BorderInfo();
            if (title.contains("心血管") || title.contains("ASCVD") || title.contains("风险")) {
                bg = Theme::CardWarn();
                border = Theme::BorderWarn();
            } else if (title.contains("BMI") || title.contains("质量指数")
                       || title.contains("体征") || title.contains("血压")
                       || title.contains("数据")) {
                bg = Theme::CardInfo();
                border = Theme::BorderInfo();
            }

            html += "<div style='" + Theme::CardStyle(bg, border) + "'>";
            html += "<h4 style='margin:0 0 8px 0; color:" + QString(border) + ";'>"
                    + esc(title) + "</h4>";
            inSection = true;
            continue;
        }

        // ---- 提示行：[提示] ----
        if (trimmed.startsWith("[提示]")) {
            html += "<p style='color:" + QString(Theme::Muted())
                 + "; font-style:italic; font-size:12px;'>"
                 + esc(trimmed.mid(4)) + "</p>";
            continue;
        }

        // ---- 报告周期行 ----
        if (trimmed.startsWith("报告周期:")) {
            html += "<p style='color:" + QString(Theme::Muted())
                 + "; font-size:13px; margin-bottom:8px;'>"
                 + esc(trimmed) + "</p>";
            continue;
        }

        // ---- 分隔符 ----
        if (trimmed == "============================================================") {
            if (inSection) {
                html += "</div>";
                inSection = false;
                currentSection.clear();
            }
            continue;
        }

        // ---- 数据行 ----
        QString styledLine = esc(trimmed);

        // 高亮风险等级关键词
        static const QVector<QPair<QString, const char*>> highlights = {
            {"偏瘦", Theme::Orange()}, {"正常", Theme::Green()},
            {"超重", Theme::Orange()},  {"肥胖", Theme::Red()},
            {"低危", Theme::Green()},   {"临界", Theme::Orange()},
            {"中危", Theme::Orange()},  {"高危", Theme::Red()},
            {"极高危", Theme::Red()},
        };
        for (const auto& [keyword, color] : highlights) {
            if (styledLine.contains(keyword)) {
                styledLine.replace(keyword,
                    QString("<b style='color:%1;'>%2</b>").arg(color, keyword));
            }
        }

        // 数值高亮（如 "BMI: 26.3"、"风险: 6.6%"）
        QRegularExpression numRe(R"((\d+\.?\d*\s*(?:%|bpm|步|小时|kg|cm|mmHg|条|)))");
        styledLine.replace(numRe,
            "<span style='color:#1565c0; font-weight:bold;'>\\1</span>");

        html += "<p style='line-height:1.8; margin:2px 0 2px 12px;'>"
                + styledLine + "</p>";
    }

    // 关闭最后的卡片
    if (inSection) {
        html += "</div>";
    }

    html += "</div>";  // 关闭外层容器
    return html;
}
