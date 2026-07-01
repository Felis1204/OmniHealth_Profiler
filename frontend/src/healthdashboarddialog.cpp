#include "healthdashboarddialog.h"
#include "ui_healthdashboarddialog.h"
#include "HealthManager.h"
#include "ASCVDCalculator.h"

HealthDashboardDialog::HealthDashboardDialog(health::HealthManager *mgr,
                                               QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::HealthDashboardDialog)
    , manager_(mgr)
{
    ui->setupUi(this);

    // ── 计算所有评估指标 ──
    double bmi  = manager_->calculateBMI();
    double ascvd = manager_->calculateASCVDScore();
    auto   tyg  = manager_->calculateTyGIndex();
    auto   cdrs = manager_->calculateCDRS();

    // ── BMI ──
    if (bmi > 0) {
        ui->bmiValue->setText(QString::number(bmi, 'f', 1));
        QString category = QString::fromStdString(manager_->getBMICategory());
        ui->bmiDesc->setText(category);

        // 颜色：正常显示绿色，否则显示橙色
        if (category == QString::fromUtf8("正常")) {
            ui->bmiValue->setStyleSheet("font-size: 22px; font-weight: bold; color: green;");
        } else {
            ui->bmiValue->setStyleSheet("font-size: 22px; font-weight: bold; color: orange;");
        }
    } else {
        ui->bmiValue->setText("—");
        ui->bmiDesc->setText("请录入身高体重");
    }

    // ── ASCVD ──
    if (ascvd > 0) {
        ui->ascvdValue->setText(QString::number(ascvd, 'f', 1) + "%");
        QString risk = QString::fromStdString(
            health::ASCVDCalculator::getRiskCategory(ascvd));
        ui->ascvdDesc->setText(risk);

        // 颜色：低危/临界绿色，中危橙色，高危/极高危红色
        QString color = "green";
        if (risk.contains("中危"))       color = "orange";
        else if (risk.contains("高危"))  color = "red";
        else if (risk.contains("极高危")) color = "red";
        ui->ascvdValue->setStyleSheet(
            QString("font-size: 22px; font-weight: bold; color: %1;").arg(color));
    } else {
        ui->ascvdValue->setText("—");
        ui->ascvdDesc->setText("请完善用户档案");
    }

    // ── TyG ──
    if (tyg.score > 0) {
        ui->tygValue->setText(QString::number(tyg.score, 'f', 2));
        ui->tygDesc->setText(QString::fromStdString(tyg.riskLevel));

        // 高风险显示红色，低风险显示绿色
        QString color = (tyg.score >= 8.70) ? "red" : "green";
        ui->tygValue->setStyleSheet(
            QString("font-size: 22px; font-weight: bold; color: %1;").arg(color));
    } else {
        ui->tygValue->setText("—");
        ui->tygDesc->setText("需要血糖+甘油三酯数据");
    }

    // ── CDRS ──
    if (cdrs.score > 0) {
        ui->cdrsValue->setText(QString::number(cdrs.score, 'f', 0) + " 分");
        ui->cdrsDesc->setText(QString::fromStdString(cdrs.riskLevel));

        // 根据分数判断颜色（男≥17 女≥14 为高风险）
        QString color = (cdrs.score >= 17) ? "red" : "green";
        // CDRS 阈值因性别而异，保守用 17 作为高风险切点
        ui->cdrsValue->setStyleSheet(
            QString("font-size: 22px; font-weight: bold; color: %1;").arg(color));
    } else {
        ui->cdrsValue->setText("—");
        ui->cdrsDesc->setText("需要年龄+腰围数据");
    }
}

HealthDashboardDialog::~HealthDashboardDialog()
{
    delete ui;
}
