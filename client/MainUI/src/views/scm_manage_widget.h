#ifndef SCM_MANAGE_WIDGET_H
#define SCM_MANAGE_WIDGET_H

#include "base/base_page_widget.h"
#include <QWidget>
#include <QHash>

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
    void on_cancel_order_button_clicked();
    void on_pushButton_clicked();
    void on_Back_btn_clicked();

private:
    Ui::ScmManageWidget *ui;

    QHash<int, QString> m_activeOrderIdByWh;
    QHash<int, quint32> m_lastQtyByWh;
    bool m_opcBound = false;

    void setupStockStatusTableConfigs();
    void setupStockOrderTableConfigs();
    void loadInventoryData();
    void loadInventoryOrderData();
    void setupOpcBindings();
};

#endif // SCM_MANAGE_WIDGET_H
