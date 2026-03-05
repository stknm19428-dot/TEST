#ifndef TEST_WIDGET_H
#define TEST_WIDGET_H

#include <QWidget>

namespace Ui {
class TestWidget;
}

class TestWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TestWidget(QWidget *parent = nullptr);
    ~TestWidget();

private:
    Ui::TestWidget *ui;

    void setupTableConfigs();
    void loadAllData();
};

#endif // TEST_WIDGET_H
