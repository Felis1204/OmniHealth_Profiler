#ifndef USERMANAGE_H
#define USERMANAGE_H

#include <QDialog>
#include "Models/UserProfile.h"

namespace health { class HealthManager; }

namespace Ui {
class UserManage;
}

class UserManage : public QDialog
{
    Q_OBJECT

public:
    explicit UserManage(health::HealthManager *mgr,
                        QWidget *parent = nullptr);
    ~UserManage();

    /// @brief 打开编辑用户档案对话框
    /// @return true 如果用户点了保存
    static bool editUserProfile(health::HealthManager *mgr,
                                QWidget *parent,
                                health::UserProfile &profile);

private slots:
    void on_newButton_clicked();
    void on_editButton_clicked();
    void on_deleteButton_clicked();
    void on_closeButton_clicked();

private:
    void loadCurrentUser();
    void showProfile(const health::UserProfile &profile);

    Ui::UserManage *ui;
    health::HealthManager *manager_;
    health::UserProfile currentProfile_;
    bool hasUser_ = false;
};

#endif // USERMANAGE_H