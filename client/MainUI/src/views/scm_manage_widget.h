#ifndef SCM_MANAGE_WIDGET_H
#define SCM_MANAGE_WIDGET_H

#include <QWidget>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

namespace Ui {
class ScmManageWidget;
}

class ScmManageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ScmManageWidget(QWidget *parent = nullptr);
    ~ScmManageWidget();

private:
    Ui::ScmManageWidget *ui;

    void setupStockStatusTableConfigs();
    void setupStockOrderTableConfigs();
    void loadInventoryData();
    void loadInventoryOrderData();
};

#endif // SCM_MANAGE_WIDGET_H
