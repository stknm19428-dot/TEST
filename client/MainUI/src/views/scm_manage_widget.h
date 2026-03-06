#ifndef SCM_MANAGE_WIDGET_H
#define SCM_MANAGE_WIDGET_H
#include "base/base_page_widget.h"
#include <QWidget>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include "logistics_schedule_dialog.h"

namespace Ui {
class ScmManageWidget;
}

class ScmManageWidget : public BasePageWidget
{
    Q_OBJECT
public:
    explicit ScmManageWidget(QWidget *parent = nullptr);
    ~ScmManageWidget();

private slots:
<<<<<<< Updated upstream
    void on_Back_btn_clicked();
    void on_create_order_button_clicked();
    void on_cancel_order_button_clicked();
=======
    void on_pushButton_clicked();
>>>>>>> Stashed changes

private:
    Ui::ScmManageWidget *ui;
    void setupStockStatusTableConfigs();
    void setupStockOrderTableConfigs();
    void loadInventoryData();
    void loadInventoryOrderData();
};

#endif // SCM_MANAGE_WIDGET_H
