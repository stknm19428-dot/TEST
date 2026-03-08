#ifndef DELIVERY_WIDGET_H
#define DELIVERY_WIDGET_H

#include <QWidget>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include "base/base_page_widget.h"
#include "delivery_dialog.h"

namespace Ui {
class DeliveryWidget;
}

class DeliveryWidget : public BasePageWidget
{
    Q_OBJECT

public:
    explicit DeliveryWidget(QWidget *parent = nullptr);
    ~DeliveryWidget();

private slots:
    void on_create_delivery_button_clicked();
    void on_complete_delivery_button_clicked();
    void on_Back_btn_clicked();

private:
    Ui::DeliveryWidget *ui;

    void setupTableConfigs();
    void loadDeliveryData();
};

#endif // DELIVERY_WIDGET_H
