#include "scm_manage_service.h"
#include "../core/user_session.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <QDebug>

QList<InventoryInfo> ScmManageService::getInventoryStatus(){
    QList<InventoryInfo> list;

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

    QSqlQuery query(
        "SELECT l.id, u.user_name, l.item_name, l.stock, l.status, "
        "l.created_at, l.receive_at, l.updated_at "
        "FROM inventory_order_logs l "
        "JOIN user u ON l.user_id = u.id "
        "ORDER BY l.created_at DESC"
        );

    while (query.next()) {
        OrderInfo info;
        info.id = query.value("id").toString();
        info.userName = query.value("user_name").toString();
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

bool ScmManageService::addOrder(const QString& userName, const QString& itemCode, int amount, const QString& dueDate) {
    Q_UNUSED(userName);

    QSqlQuery query;
    query.prepare(
        "INSERT INTO inventory_order_logs "
        "(id, user_id, item_id, item_code, item_name, stock, status, created_at, receive_at) "
        "SELECT UUID(), :userId, "
        "id, item_code, item_name, :amount, 'PENDING', NOW(), :dueDate "
        "FROM inventory WHERE item_code = :code"
        );

    query.bindValue(":userId", UserSession::instance().userId());
    query.bindValue(":amount", amount);
    query.bindValue(":code", itemCode);
    query.bindValue(":dueDate", dueDate);

    if (!query.exec()) {
        qDebug() << "addOrder failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool ScmManageService::cancelOrder(const QString& orderId) {
    QSqlQuery query;
    query.prepare("DELETE FROM inventory_order_logs WHERE id = :id");
    query.bindValue(":id", orderId);

    if (!query.exec()) {
        qDebug() << "cancelOrder failed:" << query.lastError().text();
        return false;
    }
    return true;
}

int ScmManageService::warehouseNoFromLocation(const QString& location)
{
    QString loc = location.trimmed().toUpper();
    if (loc == "WH-A01") return 1;
    if (loc == "WH-B02") return 2;
    if (loc == "WH-C03") return 3;

    return 0;
}

InboundOrderTask ScmManageService::getInboundOrderTaskById(const QString& orderId)
{
    InboundOrderTask task;

    QSqlQuery query;
    query.prepare(
        "SELECT l.id, l.item_id, l.item_code, l.item_name, l.stock, l.status, i.location "
        "FROM inventory_order_logs l "
        "JOIN inventory i ON l.item_id = i.id "
        "WHERE l.id = :id "
        "LIMIT 1"
        );
    query.bindValue(":id", orderId);

    if (!query.exec()) {
        qDebug() << "getInboundOrderTaskById failed:" << query.lastError().text();
        return task;
    }

    if (!query.next()) {
        return task;
    }

    task.valid = true;
    task.id = query.value("id").toString();
    task.itemId = query.value("item_id").toString();
    task.itemCode = query.value("item_code").toString();
    task.itemName = query.value("item_name").toString();
    task.stock = query.value("stock").toInt();
    task.status = query.value("status").toString();
    task.location = query.value("location").toString();
    task.warehouseNo = warehouseNoFromLocation(task.location);

    return task;
}

bool ScmManageService::markOrderInProc(const QString& orderId)
{
    QSqlQuery query;
    query.prepare(
        "UPDATE inventory_order_logs "
        "SET status = 'INPROC', updated_at = NOW() "
        "WHERE id = :id AND status = 'PENDING'"
        );
    query.bindValue(":id", orderId);

    if (!query.exec()) {
        qDebug() << "markOrderInProc failed:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool ScmManageService::markOrderDone(const QString& orderId)
{
    QSqlQuery query;
    query.prepare(
        "UPDATE inventory_order_logs "
        "SET status = 'DONE', updated_at = NOW() "
        "WHERE id = :id"
        );
    query.bindValue(":id", orderId);

    if (!query.exec()) {
        qDebug() << "markOrderDone failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool ScmManageService::increaseInventoryByOrderId(const QString& orderId, int delta)
{
    if (delta <= 0) return true;

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) {
        qDebug() << "increaseInventoryByOrderId transaction start failed";
        return false;
    }

    QSqlQuery q1(db);
    q1.prepare(
        "UPDATE inventory i "
        "JOIN inventory_order_logs l ON l.item_id = i.id "
        "SET i.current_stock = COALESCE(i.current_stock, 0) + :delta "
        "WHERE l.id = :id"
        );
    q1.bindValue(":delta", delta);
    q1.bindValue(":id", orderId);

    QSqlQuery q2(db);
    q2.prepare(
        "UPDATE inventory_order_logs "
        "SET updated_at = NOW() "
        "WHERE id = :id"
        );
    q2.bindValue(":id", orderId);

    if (!q1.exec() || !q2.exec()) {
        qDebug() << "increaseInventoryByOrderId failed:"
                 << q1.lastError().text() << q2.lastError().text();
        db.rollback();
        return false;
    }

    return db.commit();
}
