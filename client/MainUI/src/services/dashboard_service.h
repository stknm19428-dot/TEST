#pragma once
#include "../models/inventory_model.h"
#include <QList>
#include <QMap>
#include <QString>

struct ProductionData {
    QString date;        // 날짜 (MM-DD)
    QString product_name;
    int prod_count;
};

class DashboardService {
public:
    static QList<InventoryInfo> getStorageCharts();
    static QList<LocationInfo> getLocations();
    static QList<ProductionData> getProductionChart();
};
