#include "login_widget.h"
#include <QDebug>
#include "ui_login_widget.h" // 이 파일이 있어야 ui->... 를 쓸 수 있습니다.

LoginWidget::LoginWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LoginWidget) // 이제 Ui::LoginWidget의 크기를 알게 됩니다.
{
    ui->setupUi(this);
}

LoginWidget::~LoginWidget()
{
    delete ui;
}

void LoginWidget::on_loginBtn_clicked()
{
    QString user = ui->userEdit->text();
    QString pw = ui->passEdit->text();

    if (m_authService.authenticate(user, pw)) {
        emit loginSuccess();
    } else {
        qDebug() << "Login Failed";
    }
}
