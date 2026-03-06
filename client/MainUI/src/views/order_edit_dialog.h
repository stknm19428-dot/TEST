#ifndef ORDER_EDIT_DIALOG_H
#define ORDER_EDIT_DIALOG_H

#include <QDialog>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTimer>
#include <QDateTime>


namespace Ui {
class OrderEditDialog;
}

class OrderEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OrderEditDialog(QWidget *parent = nullptr);
    ~OrderEditDialog();

    void loadInventoryList();
    QString getSelectedItemCode() const;
    int getOrderAmount() const;

private slots:
    void updateDateTime();

private:
    Ui::OrderEditDialog *ui;

    QTimer *timer;
};

#endif // ORDER_EDIT_DIALOG_H
