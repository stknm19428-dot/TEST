#pragma once
#include "../models/manufacture_model.h"
#include <QList>

class ManufactureService {
public:
    // SELECT
    static QList<ManufactureInfo> getProducts();
    static QList<ManufactureScheduleInfo> getSchedules();

    // UPDATE
    static ProductionOrderTask getProductionOrderById(const QString &orderId);
    static QList<RecipeItem> getRecipeItemsByProductId(const QString &productId);

    static bool markProductionOrderInProc(const QString &orderId);
    static bool markProductionOrderWaitMaterial(const QString &orderId);
    static bool markProductionOrderDone(const QString &orderId);

    static bool updateProductStock(const QString &product_id, int new_stock);
    static bool updateProductLogProgress(const QString &orderId, int prodCount, int defectCount, const QString &status);

    static bool increaseProductStock(const QString &productId, int delta);
    static bool consumeRecipeItems(const QString &productId, int producedDelta);

    // INSERT
    static bool createProductLog(const ProductionOrderTask &task);

    // DELETE
    static bool deleteSchedule(const QString &orderId);
};
