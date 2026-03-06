#ifndef MANUFACTURE_WIDGET_H
#define MANUFACTURE_WIDGET_H

#include <QWidget>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include "base/base_page_widget.h"
#include "../services/manufacture_service.h"

namespace Ui {
class ManufactureWidget;
}

class ManufactureWidget : public BasePageWidget
{
    Q_OBJECT

public:
    explicit ManufactureWidget(QWidget *parent = nullptr);
    ~ManufactureWidget();

private slots:
    void on_Back_btn_clicked();
    void on_edit_stock_button_clicked();
    void on_create_schedule_button_clicked();
    void on_cancel_schedule_button_clicked();

private:
    Ui::ManufactureWidget *ui;

    // 방법 B: 배열로 id 매핑
    QList<QString> product_id_list;

    void setupManufactureTableConfigs();
    void setupScheduleTableConfigs();
    void loadManufactureData();
    void loadScheduleData();
};

#endif // MANUFACTURE_WIDGET_H
