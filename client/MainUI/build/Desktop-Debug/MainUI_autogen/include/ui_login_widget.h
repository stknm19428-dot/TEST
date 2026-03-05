/********************************************************************************
** Form generated from reading UI file 'login_widget.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LOGIN_WIDGET_H
#define UI_LOGIN_WIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_LoginWidget
{
public:
    QLineEdit *userEdit;
    QLineEdit *passEdit;
    QPushButton *loginBtn;

    void setupUi(QWidget *LoginWidget)
    {
        if (LoginWidget->objectName().isEmpty())
            LoginWidget->setObjectName("LoginWidget");
        LoginWidget->resize(680, 440);
        userEdit = new QLineEdit(LoginWidget);
        userEdit->setObjectName("userEdit");
        userEdit->setGeometry(QRect(220, 180, 113, 22));
        passEdit = new QLineEdit(LoginWidget);
        passEdit->setObjectName("passEdit");
        passEdit->setGeometry(QRect(220, 220, 113, 22));
        loginBtn = new QPushButton(LoginWidget);
        loginBtn->setObjectName("loginBtn");
        loginBtn->setGeometry(QRect(350, 180, 80, 61));

        retranslateUi(LoginWidget);

        QMetaObject::connectSlotsByName(LoginWidget);
    } // setupUi

    void retranslateUi(QWidget *LoginWidget)
    {
        LoginWidget->setWindowTitle(QCoreApplication::translate("LoginWidget", "Form", nullptr));
        loginBtn->setText(QCoreApplication::translate("LoginWidget", "Login", nullptr));
    } // retranslateUi

};

namespace Ui {
    class LoginWidget: public Ui_LoginWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LOGIN_WIDGET_H
