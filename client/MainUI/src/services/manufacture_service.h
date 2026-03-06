#pragma once
#include "../models/manufacture_model.h"
#include <QList>

class ManufactureService {
public:
    static QList<ManufactureInfo> getProducts();
    static bool updateProductStock(const QString &product_id, int new_stock);
};
