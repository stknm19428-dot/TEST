#ifndef INVENTORY_MODEL_H
#define INVENTORY_MODEL_H

#pragma once
#include <QString>

struct InventoryInfo {
    QString id;
    QString company_id;
    QString item_code;
    QString item_name;
    int current_stock;
    int min_stock_level;
    int max_stock_level;
    QString unit;
    QString location;
};

#endif // INVENTORY_MODEL_H
