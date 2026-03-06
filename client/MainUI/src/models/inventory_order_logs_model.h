#ifndef INVENTORY_ORDER_LOGS_MODEL_H
#define INVENTORY_ORDER_LOGS_MODEL_H

#pragma once
#include <QString>

struct OrderInfo { //주문 정보 구조체
    QString id;
    QString userName;
    QString itemName;
    int stock;
    QString status;
    QString createdAt;
    QString receiveAt;
    QString updatedAt;
};

#endif // INVENTORY_ORDER_LOGS_MODEL_H
