#pragma once
#include "../models/manufacture_model.h"
#include <QList>

class ManufactureService {
public:
    static QList<ManufactureInfo> getProducts();
    static bool updateProductStock(const QString &product_id, int new_stock);

    static ProductionOrderTask getProductionOrderById(const QString &orderId);
    static QList<RecipeItem> getRecipeItemsByProductId(const QString &productId);

    static bool markProductionOrderInProc(const QString &orderId);
    static bool markProductionOrderWaitMaterial(const QString &orderId);
    static bool markProductionOrderDone(const QString &orderId);

    static bool createProductLog(const ProductionOrderTask &task);
    static bool updateProductLogProgress(const QString &orderId, int prodCount, int defectCount, const QString &status);

    static bool increaseProductStock(const QString &productId, int delta);
    static bool consumeRecipeItems(const QString &productId, int producedDelta);
};
