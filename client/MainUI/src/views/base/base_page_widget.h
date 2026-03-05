#ifndef BASE_PAGE_WIDGET_H
#define BASE_PAGE_WIDGET_H

#include <QWidget>
#include "page_types.h"

class BasePageWidget : public QWidget {
    Q_OBJECT
public:
    explicit BasePageWidget(QWidget *parent = nullptr) : QWidget(parent) {}
    virtual ~BasePageWidget() {}

signals:
    // 모든 페이지 위젯이 공통으로 사용할 시그널
    // 이 페이지로 가고 싶다고 외치는 역할
    void requestPageChange(PageType targetPage);
};

#endif
