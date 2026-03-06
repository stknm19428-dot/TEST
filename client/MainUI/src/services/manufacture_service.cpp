#include "manufacture_service.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

QList<ManufactureInfo> ManufactureService::getProducts() {
    QList<ManufactureInfo> list;
    QSqlQuery query("SELECT id, product_code, product_name, product_stock, description FROM product");

    while (query.next()) {
        ManufactureInfo info;
        info.id          = query.value("id").toString();
        info.code        = query.value("product_code").toString();
        info.name        = query.value("product_name").toString();
        info.stock       = query.value("product_stock").toInt();
        info.description = query.value("description").toString();
        list.append(info);
    }
    return list;
}

bool ManufactureService::updateProductStock(const QString &product_id, int new_stock) {
    QSqlQuery query;
    query.prepare("UPDATE product SET product_stock = :stock WHERE id = :id");
    query.bindValue(":stock", new_stock);
    query.bindValue(":id",    product_id);

    if (query.exec()) {
        qDebug() << "[재고 수정 성공]" << product_id << "→" << new_stock;
        return true;
    } else {
        qDebug() << "[재고 수정 실패]" << query.lastError().text();
        return false;
    }
}
