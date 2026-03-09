#ifndef LOGIN_WIDGET_H
#define LOGIN_WIDGET_H

#include "base/base_page_widget.h"
#include <QWidget>
#include "auth_service.h" // 인증 서비스를 써야 하니까 포함

namespace Ui {
class LoginWidget;
}

class LoginWidget : public BasePageWidget
{
    Q_OBJECT

public:
    explicit LoginWidget(QWidget *parent = nullptr);
    ~LoginWidget();

signals:
    void loginSuccess(); // 로그인 성공 시 MainWindow에 알릴 신호

private slots:
    void on_loginBtn_clicked(); // UI의 버튼과 자동 연결

private:
    Ui::LoginWidget *ui;
    AuthService m_authService; // 서비스 객체 보유
};

#endif // LOGIN_WIDGET_H
