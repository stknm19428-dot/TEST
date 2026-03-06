#ifndef MANUFACTURE_SCHEDULE_DIALOG_H
#define MANUFACTURE_SCHEDULE_DIALOG_H

#include <QDialog>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

namespace Ui {
class ManufactureScheduleDialog;
}

class ManufactureScheduleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ManufactureScheduleDialog(QWidget *parent = nullptr);
    ~ManufactureScheduleDialog();

private slots:
    void on_confirm_button_clicked();
    void on_cancel_button_clicked();

private:
    Ui::ManufactureScheduleDialog *ui;

    QList<QString> product_id_list;

    void load_products();
};

#endif // MANUFACTURE_SCHEDULE_DIALOG_H
