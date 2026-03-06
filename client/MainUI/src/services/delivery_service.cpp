#include "delivery_service.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

QList<ProductInfo> DeliveryService::getProducts() {
    QList<ProductInfo> list;
    QSqlQuery query("SELECT id, product_code, product_name, product_stock FROM product");
    while (query.next()) {
        ProductInfo info;
        info.id    = query.value("id").toString();
        info.code  = query.value("product_code").toString();
        info.name  = query.value("product_name").toString();
        info.stock = query.value("product_stock").toInt();
        list.append(info);
    }
    return list;
}

QList<CompanyInfo> DeliveryService::getCustomers() {
    QList<CompanyInfo> list;
    QSqlQuery query("SELECT id, company_name, company_address, company_number FROM cust_company");
    while (query.next()) {
        CompanyInfo info;
        info.id      = query.value("id").toString();
        info.name    = query.value("company_name").toString();
        info.address = query.value("company_address").toString();
        info.contact = query.value("company_number").toString();
        list.append(info);
    }
    return list;
}

QList<DeliveryInfo> DeliveryService::getDeliveries() {
    QList<DeliveryInfo> list;
    QSqlQuery query(
        "SELECT d.id, c.company_name, p.product_name, d.delivery_stock, "
        "d.status, d.created_at, d.updated_at "
        "FROM product_deli_logs d "
        "LEFT JOIN cust_company c ON d.company_id = c.id "
        "LEFT JOIN product p ON d.product_id = p.id "
        "ORDER BY d.created_at DESC"
        );

    while (query.next()) {
        DeliveryInfo info;
        info.id             = query.value("id").toString();
        info.company_name   = query.value("company_name").toString();
        info.product_name   = query.value("product_name").toString();
        info.delivery_stock = query.value("delivery_stock").toInt();
        info.status         = query.value("status").toString();
        info.created_at     = query.value("created_at").toString();
        info.updated_at     = query.value("updated_at").toString();
        list.append(info);
    }
    return list;
}

bool DeliveryService::completeDelivery(const QString &id) {
    QSqlQuery query;
    query.prepare("UPDATE product_deli_logs SET status = 'DONE', updated_at = NOW() WHERE id = :id");
    query.bindValue(":id", id);

    if (query.exec()) {
        qDebug() << "[납품 완료 성공]" << id;
        return true;
    } else {
        qDebug() << "[납품 완료 실패]" << query.lastError().text();
        return false;
    }
}
