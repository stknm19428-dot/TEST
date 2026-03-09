#ifndef DASHBOARD_WIDGET_H
#define DASHBOARD_WIDGET_H

#include "base/base_page_widget.h"
#include "../services/opcua_service.h"
#include <QtCharts/QChartView>
#include <QtCharts/QPieSeries>
#include <QtCharts/QStackedBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QChart>
#include <QChart>
#include <QShowEvent>
#include <QLabel>
#include <QGridLayout>

namespace Ui {
class DashboardWidget;
}

class DashboardWidget : public BasePageWidget {
    Q_OBJECT
public:
    explicit DashboardWidget(QWidget *parent = nullptr);
    ~DashboardWidget();

    void set_opcua_service(OpcUaService *service);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void on_CompanyListBtn_clicked();
    void on_ScmManageBtn_clicked();
    void on_DeliveryBtn_clicked();
    void on_ProcessBtn_clicked();
    void on_ManufactureBtn_clicked();
    void on_ErrorLogBtn_clicked();

    void on_mfg_temp_updated(double temp);
    void on_mfg_hum_updated(double hum);
    void on_log_temp_updated(double temp);
    void on_log_hum_updated(double hum);

private:
    Ui::DashboardWidget *ui;
    void initStorageCharts();
    void initProductionChart();
    void initSensorWidget();
    void clearLayout(QLayout *layout);

    OpcUaService *m_opcua_service = nullptr;

    QLabel *mfg_temp_value = nullptr;
    QLabel *mfg_hum_value  = nullptr;
    QLabel *log_temp_value = nullptr;
    QLabel *log_hum_value  = nullptr;
};

#endif
