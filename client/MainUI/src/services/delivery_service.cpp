#include "delivery_service.h"
#include <QSqlQuery>

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
