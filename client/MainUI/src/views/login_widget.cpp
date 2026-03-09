#include "login_widget.h"
#include <QDebug>
#include "ui_login_widget.h"

LoginWidget::LoginWidget(QWidget *parent)
    : BasePageWidget(parent)
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

    // Web으로 비유 => HTTP Response
    if (m_authService.authenticate(user, pw)) {
        // 200 OK
        qDebug() << "Login Success!";
        emit loginSuccess();
        emit requestPageChange(PageType::Dashboard);
    } else {
        qDebug() << "Login Failed";
    }

}
