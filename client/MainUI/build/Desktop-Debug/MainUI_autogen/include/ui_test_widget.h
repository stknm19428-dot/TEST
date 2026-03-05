/********************************************************************************
** Form generated from reading UI file 'test_widget.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TEST_WIDGET_H
#define UI_TEST_WIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TestWidget
{
public:
    QWidget *gridLayoutWidget;
    QGridLayout *gridLayout;
    QTableWidget *tableSuppCompany;
    QLabel *SuppLabel;
    QTableWidget *tableCustCompany;
    QLabel *CustLabel;

    void setupUi(QWidget *TestWidget)
    {
        if (TestWidget->objectName().isEmpty())
            TestWidget->setObjectName("TestWidget");
        TestWidget->resize(680, 440);
        gridLayoutWidget = new QWidget(TestWidget);
        gridLayoutWidget->setObjectName("gridLayoutWidget");
        gridLayoutWidget->setGeometry(QRect(0, 30, 541, 391));
        gridLayout = new QGridLayout(gridLayoutWidget);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setContentsMargins(0, 0, 0, 0);
        tableSuppCompany = new QTableWidget(gridLayoutWidget);
        tableSuppCompany->setObjectName("tableSuppCompany");

        gridLayout->addWidget(tableSuppCompany, 2, 0, 1, 1);

        SuppLabel = new QLabel(gridLayoutWidget);
        SuppLabel->setObjectName("SuppLabel");

        gridLayout->addWidget(SuppLabel, 0, 0, 1, 1);

        tableCustCompany = new QTableWidget(gridLayoutWidget);
        tableCustCompany->setObjectName("tableCustCompany");

        gridLayout->addWidget(tableCustCompany, 4, 0, 1, 1);

        CustLabel = new QLabel(gridLayoutWidget);
        CustLabel->setObjectName("CustLabel");

        gridLayout->addWidget(CustLabel, 3, 0, 1, 1);


        retranslateUi(TestWidget);

        QMetaObject::connectSlotsByName(TestWidget);
    } // setupUi

    void retranslateUi(QWidget *TestWidget)
    {
        TestWidget->setWindowTitle(QCoreApplication::translate("TestWidget", "Form", nullptr));
        SuppLabel->setText(QCoreApplication::translate("TestWidget", "Supporter Lists", nullptr));
        CustLabel->setText(QCoreApplication::translate("TestWidget", "Customer Lists", nullptr));
    } // retranslateUi

};

namespace Ui {
    class TestWidget: public Ui_TestWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TEST_WIDGET_H
