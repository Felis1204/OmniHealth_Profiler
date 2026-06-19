#include "usermanage.h"
#include "ui_usermanage.h"
#include "HealthManager.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QDateEdit>
#include <QMessageBox>
#include <QUuid>


// 编辑用户档案对话框
bool UserManage::editUserProfile(health::HealthManager *mgr,
                                  QWidget *parent,
                                  health::UserProfile &profile)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(profile.id.empty() ? "新增用户" : "编辑用户");
    dlg.setMinimumWidth(350);

    auto *layout = new QFormLayout(&dlg);

    auto *nameEdit = new QLineEdit(QString::fromStdString(profile.name));
    nameEdit->setPlaceholderText("必填");
    layout->addRow("姓名：", nameEdit);

    auto *birthEdit = new QDateEdit;
    birthEdit->setCalendarPopup(true);
    birthEdit->setSpecialValueText("未设置");
    birthEdit->setDate(QDate::currentDate());
    if (profile.birthDate) {
        birthEdit->setDate(QDate::fromString(
            QString::fromStdString(*profile.birthDate), "yyyy-MM-dd"));
    }
    layout->addRow("出生日期：", birthEdit);

    auto *genderCombo = new QComboBox;
    genderCombo->addItems({"未设置", "男", "女"});
    if (profile.gender) {
        genderCombo->setCurrentText(
            *profile.gender == "MALE" ? "男" :
            *profile.gender == "FEMALE" ? "女" : "未设置");
    }
    layout->addRow("性别：", genderCombo);

    auto *smokingCombo = new QComboBox;
    smokingCombo->addItems({"未设置", "从不吸烟", "已戒烟", "当前吸烟"});
    if (profile.smokingStatus) {
        if (*profile.smokingStatus == "NEVER")   smokingCombo->setCurrentIndex(1);
        if (*profile.smokingStatus == "FORMER")  smokingCombo->setCurrentIndex(2);
        if (*profile.smokingStatus == "CURRENT") smokingCombo->setCurrentIndex(3);
    }
    layout->addRow("吸烟状态：", smokingCombo);

    auto *regionCombo = new QComboBox;
    regionCombo->addItems({"未设置", "北方", "南方"});
    if (profile.region) {
        regionCombo->setCurrentText(
            *profile.region == "NORTH" ? "北方" :
            *profile.region == "SOUTH" ? "南方" : "未设置");
    }
    layout->addRow("地域：", regionCombo);

    auto *urbanCombo = new QComboBox;
    urbanCombo->addItems({"未设置", "城市", "农村"});
    if (profile.urbanRural) {
        urbanCombo->setCurrentText(
            *profile.urbanRural == "URBAN" ? "城市" :
            *profile.urbanRural == "RURAL" ? "农村" : "未设置");
    }
    layout->addRow("城乡：", urbanCombo);

    auto *familyHistoryCheck = new QCheckBox("有早发 ASCVD 家族史");
    if (profile.familyHistoryASCVD && *profile.familyHistoryASCVD)
        familyHistoryCheck->setChecked(true);
    layout->addRow("", familyHistoryCheck);

    auto *diabetesCheck = new QCheckBox("确诊糖尿病");
    if (profile.hasDiabetes && *profile.hasDiabetes)
        diabetesCheck->setChecked(true);
    layout->addRow("", diabetesCheck);

    // 按钮
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("保存");
    layout->addRow(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, [&]() {
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(&dlg, "提示", "姓名不能为空");
            return;
        }

        profile.name = name.toStdString();

        QDate birth = birthEdit->date();
        profile.birthDate = birth.toString("yyyy-MM-dd").toStdString();

        int gIdx = genderCombo->currentIndex();
        if (gIdx == 0) profile.gender = std::nullopt;
        else profile.gender = (gIdx == 1) ? "MALE" : "FEMALE";

        int sIdx = smokingCombo->currentIndex();
        if (sIdx == 0) profile.smokingStatus = std::nullopt;
        else if (sIdx == 1) profile.smokingStatus = "NEVER";
        else if (sIdx == 2) profile.smokingStatus = "FORMER";
        else profile.smokingStatus = "CURRENT";

        int rIdx = regionCombo->currentIndex();
        if (rIdx == 0) profile.region = std::nullopt;
        else profile.region = (rIdx == 1) ? "NORTH" : "SOUTH";

        int uIdx = urbanCombo->currentIndex();
        if (uIdx == 0) profile.urbanRural = std::nullopt;
        else profile.urbanRural = (uIdx == 1) ? "URBAN" : "RURAL";

        profile.familyHistoryASCVD = std::make_optional(familyHistoryCheck->isChecked());
        profile.hasDiabetes = std::make_optional(diabetesCheck->isChecked());

        dlg.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    return dlg.exec() == QDialog::Accepted;
}


// 构造 / 析构
UserManage::UserManage(health::HealthManager *mgr, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::UserManage)
    , manager_(mgr)
{
    ui->setupUi(this);
    loadCurrentUser();
}

UserManage::~UserManage()
{
    delete ui;
}


// 加载
void UserManage::loadCurrentUser()
{
    hasUser_ = false;
    auto opt = manager_->getUserProfile();
    if (opt) {
        currentProfile_ = *opt;
        hasUser_ = true;
        showProfile(currentProfile_);
        ui->newButton->setEnabled(false);
        ui->editButton->setEnabled(true);
        ui->deleteButton->setEnabled(true);
    } else {
        currentProfile_ = health::UserProfile();
        ui->nameValue->setText("（无）");
        ui->genderValue->setText("—");
        ui->birthValue->setText("—");
        ui->smokingValue->setText("—");
        ui->regionValue->setText("—");
        ui->diabetesValue->setText("—");
        ui->newButton->setEnabled(true);
        ui->editButton->setEnabled(false);
        ui->deleteButton->setEnabled(false);
    }
}

void UserManage::showProfile(const health::UserProfile &p)
{
    ui->nameValue->setText(QString::fromStdString(p.name));
    if (p.gender)
        ui->genderValue->setText(*p.gender == "MALE" ? "男" : "女");
    if (p.birthDate)
        ui->birthValue->setText(QString::fromStdString(*p.birthDate));
    if (p.smokingStatus) {
        if (*p.smokingStatus == "NEVER")   ui->smokingValue->setText("从不吸烟");
        if (*p.smokingStatus == "FORMER")  ui->smokingValue->setText("已戒烟");
        if (*p.smokingStatus == "CURRENT") ui->smokingValue->setText("当前吸烟");
    }
    if (p.region)
        ui->regionValue->setText(*p.region == "NORTH" ? "北方" : "南方");
    if (p.hasDiabetes && *p.hasDiabetes)
        ui->diabetesValue->setText("有");
    else
        ui->diabetesValue->setText("无");

    if (p.urbanRural)
        ui->urbanValue->setText(*p.urbanRural == "URBAN" ? "城市" : "农村");
    if (p.familyHistoryASCVD && *p.familyHistoryASCVD)
        ui->familyValue->setText("有早发家族史");
    else
        ui->familyValue->setText("无");
}


// 新增
void UserManage::on_newButton_clicked()
{
    health::UserProfile profile;
    profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();

    if (editUserProfile(manager_, this, profile)) {
        manager_->saveUserProfile(profile);
        QMessageBox::information(this, "成功", "用户已创建");
        loadCurrentUser();
    }
}


// 编辑
void UserManage::on_editButton_clicked()
{
    if (!hasUser_) {
        QMessageBox::information(this, "提示", "当前没有用户，请先新增");
        return;
    }

    if (editUserProfile(manager_, this, currentProfile_)) {
        manager_->saveUserProfile(currentProfile_);
        QMessageBox::information(this, "成功", "用户信息已更新");
        loadCurrentUser();
    }
}


// 删除
void UserManage::on_deleteButton_clicked()
{
    if (!hasUser_) {
        QMessageBox::information(this, "提示", "当前没有用户");
        return;
    }

    auto btn = QMessageBox::question(this, "确认删除",
        QString("确定要删除用户「%1」吗？\n此操作不可撤销。")
            .arg(QString::fromStdString(currentProfile_.name)));
    if (btn != QMessageBox::Yes) return;

    if (manager_->deleteUserProfile(currentProfile_.id)) {
        QMessageBox::information(this, "成功", "用户已删除");
        loadCurrentUser();
    } else {
        QMessageBox::warning(this, "失败", "删除失败，请重试");
    }
}


// close()
void UserManage::on_closeButton_clicked()
{
    close();
}