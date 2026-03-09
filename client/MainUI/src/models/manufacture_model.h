#pragma once
#include <QString>
#include <QList>

struct ManufactureInfo {
    QString id;
    QString code;
    QString name;
    int stock = 0;
    QString description;
};

struct RecipeItem {
    QString itemId;
    QString itemCode;
    QString itemName;
    int quantityRequired = 0;
    QString location;
    int warehouseNo = 0;
};

struct ProductionOrderTask {
    bool valid = false;
    QString orderId;
    QString productId;
    QString productCode;
    QString productName;
    int orderCount = 0;
    int motorSpeed = 100;
    QString status;
    int productNo = 1;
};
