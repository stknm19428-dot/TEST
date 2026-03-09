#ifndef SCM_MANAGE_SERVICE_H
#define SCM_MANAGE_SERVICE_H

#include "../models/inventory_model.h"
#include "../models/inventory_order_logs_model.h"
#include <QString>
#include <QList>
#include <QVariant>

struct InboundOrderTask {
    bool valid = false;
    QString id;
    QString itemId;
    QString itemCode;
    QString itemName;
    int stock = 0;
    QString status;
    QString location;
    int warehouseNo = 0;
};

struct WarehouseStockSnapshot {
    quint32 wh1 = 0;
    quint32 wh2 = 0;
    quint32 wh3 = 0;
};

class ScmManageService
{
public:
    static QList<InventoryInfo> getInventoryStatus();
    static QList<OrderInfo> getOrderLogs();
    static bool addOrder(const QString& userName, const QString& itemCode, int amount, const QString& dueDate);
    static bool cancelOrder(const QString& orderId);

    static InboundOrderTask getInboundOrderTaskById(const QString& orderId);
    static int warehouseNoFromLocation(const QString& location);
    static WarehouseStockSnapshot getWarehouseStockSnapshot();

    static bool markOrderInProc(const QString& orderId);
    static bool markOrderDone(const QString& orderId);
    static bool increaseInventoryByOrderId(const QString& orderId, int delta);
};

#endif // SCM_MANAGE_SERVICE_H
