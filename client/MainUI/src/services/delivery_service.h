#pragma once
#include "../models/product_model.h"
#include "../models/company_model.h"
#include <QList>

class DeliveryService {
public:
    static QList<ProductInfo> getProducts();
    static QList<CompanyInfo> getCustomers();
};
