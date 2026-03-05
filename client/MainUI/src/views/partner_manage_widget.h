#ifndef PARTNER_MANAGE_WIDGET_H
#define PARTNER_MANAGE_WIDGET_H

#include <QWidget>

namespace Ui {
class PartnerManageWidget;
}

class PartnerManageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PartnerManageWidget(QWidget *parent = nullptr);
    ~PartnerManageWidget();

private:
    Ui::PartnerManageWidget *ui;

    void setupTableConfigs();
    void loadAllData();
};

#endif // PARTNER_MANAGE_WIDGET_H
