/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QStackedWidget *stackedWidget;
    QWidget *loginPage;
    QLineEdit *passEdit;
    QPushButton *loginBtn;
    QLineEdit *userEdit;
    QWidget *chartPage;
    QWidget *chartWidget;
    QWidget *verticalLayoutWidget;
    QVBoxLayout *chartLayout;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(800, 600);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        stackedWidget = new QStackedWidget(centralwidget);
        stackedWidget->setObjectName("stackedWidget");
        stackedWidget->setGeometry(QRect(20, 10, 411, 261));
        loginPage = new QWidget();
        loginPage->setObjectName("loginPage");
        passEdit = new QLineEdit(loginPage);
        passEdit->setObjectName("passEdit");
        passEdit->setGeometry(QRect(30, 70, 113, 22));
        loginBtn = new QPushButton(loginPage);
        loginBtn->setObjectName("loginBtn");
        loginBtn->setGeometry(QRect(170, 31, 80, 61));
        userEdit = new QLineEdit(loginPage);
        userEdit->setObjectName("userEdit");
        userEdit->setGeometry(QRect(30, 30, 113, 22));
        stackedWidget->addWidget(loginPage);
        chartPage = new QWidget();
        chartPage->setObjectName("chartPage");
        chartWidget = new QWidget(chartPage);
        chartWidget->setObjectName("chartWidget");
        chartWidget->setGeometry(QRect(30, 20, 311, 221));
        verticalLayoutWidget = new QWidget(chartWidget);
        verticalLayoutWidget->setObjectName("verticalLayoutWidget");
        verticalLayoutWidget->setGeometry(QRect(20, 10, 261, 191));
        chartLayout = new QVBoxLayout(verticalLayoutWidget);
        chartLayout->setObjectName("chartLayout");
        chartLayout->setContentsMargins(0, 0, 0, 0);
        stackedWidget->addWidget(chartPage);
        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 800, 19));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        loginBtn->setText(QCoreApplication::translate("MainWindow", "Login", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
