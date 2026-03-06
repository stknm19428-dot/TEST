#ifndef SCM_MANAGE_SERVICE_H
#define SCM_MANAGE_SERVICE_H
#include "../models/inventory_model.h"
#include "../models/inventory_order_logs_model.h"
#include <QString>
#include <QList>
#include <QVariant>

class ScmManageService
{
public:
    static QList<InventoryInfo> getInventoryStatus();
    static QList<OrderInfo> getOrderLogs();
    static bool addOrder(const QString& userName, const QString& itemCode, int amount);
};

#endif // SCM_MANAGE_SERVICE_H
