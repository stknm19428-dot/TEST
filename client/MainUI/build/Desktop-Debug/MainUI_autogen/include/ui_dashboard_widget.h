/********************************************************************************
** Form generated from reading UI file 'dashboard_widget.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DASHBOARD_WIDGET_H
#define UI_DASHBOARD_WIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_DashboardWidget
{
public:
    QWidget *verticalLayoutWidget;
    QVBoxLayout *chartLayout;
    QPushButton *CompanyListBtn;

    void setupUi(QWidget *DashboardWidget)
    {
        if (DashboardWidget->objectName().isEmpty())
            DashboardWidget->setObjectName("DashboardWidget");
        DashboardWidget->resize(680, 440);
        verticalLayoutWidget = new QWidget(DashboardWidget);
        verticalLayoutWidget->setObjectName("verticalLayoutWidget");
        verticalLayoutWidget->setGeometry(QRect(9, 9, 171, 131));
        chartLayout = new QVBoxLayout(verticalLayoutWidget);
        chartLayout->setObjectName("chartLayout");
        chartLayout->setContentsMargins(0, 0, 0, 0);
        CompanyListBtn = new QPushButton(DashboardWidget);
        CompanyListBtn->setObjectName("CompanyListBtn");
        CompanyListBtn->setGeometry(QRect(70, 390, 80, 22));

        retranslateUi(DashboardWidget);

        QMetaObject::connectSlotsByName(DashboardWidget);
    } // setupUi

    void retranslateUi(QWidget *DashboardWidget)
    {
        DashboardWidget->setWindowTitle(QCoreApplication::translate("DashboardWidget", "Form", nullptr));
        CompanyListBtn->setText(QCoreApplication::translate("DashboardWidget", "Comp.List", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DashboardWidget: public Ui_DashboardWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DASHBOARD_WIDGET_H
