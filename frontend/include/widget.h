#pragma once

#include <QWidget>
#include <memory>

namespace health { class HealthManager; }

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;

private slots:
    void on_pushButton_clicked();
    void on_AddDataButton_clicked();
    void on_AddDataButton_2_clicked();
    void on_usermanagebutton_clicked();

private:
    Ui::Widget *ui;
    std::unique_ptr<health::HealthManager> manager_;
};
