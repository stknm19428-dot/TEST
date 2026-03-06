#include "scm_manage_service.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>


QList<InventoryInfo> ScmManageService::getInventoryStatus(){
    QList<InventoryInfo> list;
    // 재고 현황을 가져오는 쿼리
    QSqlQuery query("SELECT id, item_code, item_name, current_stock, location FROM inventory");

    while (query.next()) {
        InventoryInfo info;
        info.id = query.value("id").toString();
        info.item_code = query.value("item_code").toString();
        info.item_name = query.value("item_name").toString();
        info.current_stock = query.value("current_stock").toInt();
        info.location = query.value("location").toString();
        list.append(info);
    }
    return list;
}
QList<OrderInfo> ScmManageService::getOrderLogs(){
    QList<OrderInfo> list;
    QSqlQuery query("SELECT id, user_id, item_name, stock, status, created_at, receive_at, updated_at "
                    "FROM inventory_order_logs ORDER BY created_at DESC");

    while (query.next()) {
        OrderInfo info;
        info.id = query.value("id").toString();
        info.userName = query.value("user_id").toString(); // 나중에 이름 조인 가능, 수정필요
        info.itemName = query.value("item_name").toString();
        info.stock = query.value("stock").toInt();
        info.status = query.value("status").toString();
        info.createdAt = query.value("created_at").toString();
        info.receiveAt = query.value("receive_at").toString();
        info.updatedAt = query.value("updated_at").toString();

        list.append(info);
    }
    return list;
}

bool ScmManageService::addOrder(const QString& userName, const QString& itemCode, int amount) {
    QSqlQuery query;
    query.prepare("INSERT INTO inventory_order_logs (id, user_id, item_id, item_code, item_name, stock, status, created_at) "
                  "SELECT UUID_v4(), (SELECT id FROM user WHERE user_name = :test), "
                  "id, item_code, item_name, :amount, 'PENDING', NOW() "
                  "FROM inventory WHERE item_code = :code");

    query.bindValue(":userName", userName);
    query.bindValue(":amount", amount);
    query.bindValue(":code", itemCode);

    return query.exec();
}
