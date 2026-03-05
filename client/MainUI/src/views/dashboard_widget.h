#ifndef DASHBOARD_WIDGET_H
#define DASHBOARD_WIDGET_H

#include "base/base_page_widget.h"

#include <QtCharts/QChartView>
#include <QtCharts/QPieSeries>
#include <QtCharts/QStackedBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QChart>
#include <QChart>

#include <QShowEvent>

namespace Ui {
class DashboardWidget;
}

class DashboardWidget : public BasePageWidget {
    Q_OBJECT

public:
    explicit DashboardWidget(QWidget *parent = nullptr);
    ~DashboardWidget();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void on_CompanyListBtn_clicked();

private:
    Ui::DashboardWidget *ui;

    void initStorageCharts();
    void initProductionChart();
    void clearLayout(QLayout *layout);
};

#endif