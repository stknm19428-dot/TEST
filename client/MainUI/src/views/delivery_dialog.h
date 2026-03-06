#ifndef DELIVERY_DIALOG_H
#define DELIVERY_DIALOG_H

#include <QDialog>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include "../services/delivery_service.h"

namespace Ui {
class DeliveryDialog;
}

class DeliveryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DeliveryDialog(QWidget *parent = nullptr);
    ~DeliveryDialog();

private slots:
    void on_confirm_button_clicked();
    void on_cancel_button_clicked();

private:
    Ui::DeliveryDialog *ui;

    // 방법 B: 배열로 id, code 매핑
    QList<QString> product_id_list;
    QList<QString> product_code_list;
    QList<QString> company_id_list;
    QList<QString> company_name_list;

    void load_products();
    void load_customers();
};

#endif // DELIVERY_DIALOG_H
