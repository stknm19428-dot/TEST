#pragma once
#include "../models/product_model.h"
#include "../models/company_model.h"
#include <QList>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

struct DeliveryInfo {
    QString id;
    QString company_name;
    QString product_name;
    int delivery_stock;
    QString status;
    QString created_at;
    QString updated_at;
};

class DeliveryService {
public:
    static QList<ProductInfo> getProducts();
    static QList<CompanyInfo> getCustomers();
    static QList<DeliveryInfo> getDeliveries();
    static bool completeDelivery(const QString &id);
};
