#ifndef PARTNER_MANAGE_WIDGET_H
#define PARTNER_MANAGE_WIDGET_H

#include <QWidget>
#include "base/base_page_widget.h"

namespace Ui {
class PartnerManageWidget;
}

class PartnerManageWidget : public BasePageWidget
{
    Q_OBJECT

public:
    explicit PartnerManageWidget(QWidget *parent = nullptr);
    ~PartnerManageWidget();

private slots:
    void on_Back_btn_clicked();

private:
    Ui::PartnerManageWidget *ui;

    void setupTableConfigs();
    void loadAllData();
};

#endif // PARTNER_MANAGE_WIDGET_H
