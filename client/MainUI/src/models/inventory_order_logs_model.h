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