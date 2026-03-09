#include "manufacture_service.h"
#include "../core/user_session.h"
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

// ==============================
// SELECT
// ==============================

QList<ManufactureInfo> ManufactureService::getProducts() {
    QList<ManufactureInfo> list;
    QSqlQuery query(
        "SELECT id, product_code, product_name, product_stock, description " 
        "FROM product");

    while (query.next()) {
        ManufactureInfo info;
        info.id = query.value("id").toString();
        info.code = query.value("product_code").toString();
        info.name = query.value("product_name").toString();
        info.stock = query.value("product_stock").toInt();
        info.description = query.value("description").toString();
        list.append(info);
    }
    return list;
}

ProductionOrderTask ManufactureService::getProductionOrderById(const QString &orderId)
{
    ProductionOrderTask task;

    QSqlQuery query;
    query.prepare(
        "SELECT o.id, o.product_id, o.order_count, o.motor_speed, o.status, "
        "p.product_code, p.product_name "
        "FROM product_order_logs o "
        "JOIN product p ON o.product_id = p.id "
        "WHERE o.id = :id "
        "LIMIT 1");
    query.bindValue(":id", orderId);

    if (!query.exec()) {
        qDebug() << "getProductionOrderById failed:" << query.lastError().text();
        return task;
    }

    if (!query.next())
        return task;

    task.valid = true;
    task.orderId = query.value("id").toString();
    task.productId = query.value("product_id").toString();
    task.productCode = query.value("product_code").toString();
    task.productName = query.value("product_name").toString();
    task.orderCount = query.value("order_count").toInt();
    task.motorSpeed = query.value("motor_speed").toInt();
    task.status = query.value("status").toString();

    QRegularExpression re("(\\d+)$");
    auto match = re.match(task.productCode);
    task.productNo = match.hasMatch() ? match.captured(1).toInt() : 1;
    if (task.productNo <= 0)
        task.productNo = 1;

    return task;
}

QList<RecipeItem> ManufactureService::getRecipeItemsByProductId(const QString &productId)
{
    QList<RecipeItem> list;

    QSqlQuery query;
    query.prepare(
        "SELECT pi.item_id, i.item_code, i.item_name, pi.quantity_required, i.location "
        "FROM product_items pi "
        "JOIN inventory i ON pi.item_id = i.id "
        "WHERE pi.product_id = :productId");
    query.bindValue(":productId", productId);

    if (!query.exec()) {
        qDebug() << "getRecipeItemsByProductId failed:" << query.lastError().text();
        return list;
    }

    while (query.next()) {
        RecipeItem item;
        item.itemId = query.value("item_id").toString();
        item.itemCode = query.value("item_code").toString();
        item.itemName = query.value("item_name").toString();
        item.quantityRequired = query.value("quantity_required").toInt();
        item.location = query.value("location").toString();

        const QString loc = item.location.trimmed().toUpper();
        if (loc == "WH-A01") item.warehouseNo = 1;
        else if (loc == "WH-B02") item.warehouseNo = 2;
        else if (loc == "WH-C03") item.warehouseNo = 3;
        else item.warehouseNo = 0;

        list.append(item);
    }

    return list;
}

QList<ManufactureScheduleInfo> ManufactureService::getSchedules() {
    QList<ManufactureScheduleInfo> list;
    QSqlQuery query(
        "SELECT p.product_code, p.product_name, o.id, o.order_count, o.motor_speed, "
        "o.status, o.created_at, o.deadline_at, o.updated_at "
        "FROM product_order_logs o "
        "LEFT JOIN product p ON o.product_id = p.id "
        "ORDER BY o.created_at DESC");

    while (query.next()) {
        ManufactureScheduleInfo info;
        info.id = query.value("id").toString();
        info.productCode = query.value("product_code").toString();
        info.productName = query.value("product_name").toString();
        info.orderCount = query.value("order_count").toInt();
        info.motorSpeed = query.value("motor_speed").toInt();
        info.status = query.value("status").toString();
        info.createdAt = query.value("created_at").toString();
        info.deadlineAt = query.value("deadline_at").toString();
        info.updatedAt = query.value("updated_at").toString();
        list.append(info);
    }
    return list;
}

// ==============================
// UPDATE
// ==============================

bool ManufactureService::updateProductStock(const QString &product_id, int new_stock) {
    QSqlQuery query;
    query.prepare(
        "UPDATE product "
        "SET product_stock = :stock "
        "WHERE id = :id");
    query.bindValue(":stock", new_stock);
    query.bindValue(":id", product_id);

    if (query.exec()) {
        qDebug() << "[재고 수정 성공]" << product_id << "→" << new_stock;
        return true;
    }

    qDebug() << "[재고 수정 실패]" << query.lastError().text();
    return false;
}

bool ManufactureService::markProductionOrderInProc(const QString &orderId)
{
    QSqlQuery query;
    query.prepare(
        "UPDATE product_order_logs "
        "SET status = 'INPROC', updated_at = NOW() "
        "WHERE id = :id AND status = 'PENDING'");
    query.bindValue(":id", orderId);

    if (!query.exec()) {
        qDebug() << "markProductionOrderInProc failed:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool ManufactureService::markProductionOrderWaitMaterial(const QString &orderId)
{
    QSqlQuery query;
    query.prepare(
        "UPDATE product_order_logs "
        "SET status = 'WAIT_MAT', updated_at = NOW() "
        "WHERE id = :id");
    query.bindValue(":id", orderId);

    if (!query.exec()) {
        qDebug() << "markProductionOrderWaitMaterial failed:" << query.lastError().text();
        return false;
    }

    return true;
}

bool ManufactureService::markProductionOrderDone(const QString &orderId)
{
    QSqlQuery query;
    query.prepare(
        "UPDATE product_order_logs "
        "SET status = 'DONE', updated_at = NOW() "
        "WHERE id = :id");
    query.bindValue(":id", orderId);

    if (!query.exec()) {
        qDebug() << "markProductionOrderDone failed:" << query.lastError().text();
        return false;
    }

    return true;
}

bool ManufactureService::updateProductLogProgress(const QString &orderId, int prodCount, int defectCount, const QString &status)
{
    QSqlQuery query;
    query.prepare(
        "UPDATE product_logs "
        "SET prod_count = :prodCount, defect_count = :defectCount, status = :status, "
        "ended_at = CASE WHEN :status = 'DONE' THEN NOW() ELSE ended_at END "
        "WHERE order_id = :orderId");
    query.bindValue(":prodCount", prodCount);
    query.bindValue(":defectCount", defectCount);
    query.bindValue(":status", status);
    query.bindValue(":orderId", orderId);

    if (!query.exec()) {
        qDebug() << "updateProductLogProgress failed:" << query.lastError().text();
        return false;
    }

    return true;
}

bool ManufactureService::increaseProductStock(const QString &productId, int delta)
{
    if (delta <= 0) return true;

    QSqlQuery query;
    query.prepare(
        "UPDATE product "
        "SET product_stock = COALESCE(product_stock, 0) + :delta "
        "WHERE id = :id");
    query.bindValue(":delta", delta);
    query.bindValue(":id", productId);

    if (!query.exec()) {
        qDebug() << "increaseProductStock failed:" << query.lastError().text();
        return false;
    }

    return true;
}

bool ManufactureService::consumeRecipeItems(const QString &productId, int producedDelta)
{
    if (producedDelta <= 0) return true;

    const auto recipe = getRecipeItemsByProductId(productId);
    if (recipe.isEmpty()) {
        qDebug() << "consumeRecipeItems: recipe empty for productId =" << productId;
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) {
        qDebug() << "consumeRecipeItems transaction start failed";
        return false;
    }

    for (const auto &item : recipe) {
        const int consumeQty = item.quantityRequired * producedDelta;

        QSqlQuery query(db);
        query.prepare(
            "UPDATE inventory "
            "SET current_stock = COALESCE(current_stock, 0) - :consumeQty "
            "WHERE id = :itemId "
            "AND COALESCE(current_stock, 0) >= :consumeQty");
        query.bindValue(":consumeQty", consumeQty);
        query.bindValue(":itemId", item.itemId);

        if (!query.exec() || query.numRowsAffected() <= 0) {
            qDebug() << "consumeRecipeItems failed for item =" << item.itemCode
                     << "err =" << query.lastError().text();
            db.rollback();
            return false;
        }
    }

    return db.commit();
}


// ==============================
// INSERT
// ==============================

bool ManufactureService::createProductLog(const ProductionOrderTask &task)
{
    QSqlQuery query;
    query.prepare(
        "INSERT INTO product_logs "
        "(id, order_id, user_id, assignment_part, motor_speed, prod_count, defect_count, status, started_at) "
        "VALUES (UUID(), :orderId, :userId, 'MFG', :motorSpeed, 0, 0, 'INPROC', NOW())");
    query.bindValue(":orderId", task.orderId);
    query.bindValue(":userId", UserSession::instance().userId().isEmpty() ? QVariant(QVariant::String) : QVariant(UserSession::instance().userId()));
    query.bindValue(":motorSpeed", task.motorSpeed);

    if (!query.exec()) {
        qDebug() << "createProductLog failed:" << query.lastError().text();
        return false;
    }

    return true;
}

// ==============================
// DELETE
// ==============================


bool ManufactureService::deleteSchedule(const QString &orderId) {
    QSqlQuery query;
    query.prepare("DELETE FROM product_order_logs WHERE id = :id AND status != 'DONE'");
    query.bindValue(":id", orderId);

    if (query.exec()) {
        return query.numRowsAffected() > 0;
    }
    qDebug() << "Delete Schedule Error:" << query.lastError().text();
    return false;
}