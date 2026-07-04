#include "healthdashboarddialog.h"
#include "ui_healthdashboarddialog.h"
#include "HealthManager.h"
#include "ASCVDCalculator.h"
#include "Theme.h"

HealthDashboardDialog::HealthDashboardDialog(health::HealthManager *mgr,
                                               QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::HealthDashboardDialog)
    , manager_(mgr)
{
    ui->setupUi(this);

    // 统一主题
    setStyleSheet(Theme::DialogBase());

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
        const char* color = Theme::riskColor(category);
        ui->bmiValue->setStyleSheet(
            QString("font-size: 22px; font-weight: bold; color: %1;").arg(color));
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
        const char* color = Theme::riskColor(risk);
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
        const char* color = (tyg.score >= 8.70) ? Theme::Danger() : Theme::Okay();
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
        const char* color = (cdrs.score >= 17) ? Theme::Danger() : Theme::Okay();
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
