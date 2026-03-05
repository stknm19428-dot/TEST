#ifndef DASHBOARD_WIDGET_H
#define DASHBOARD_WIDGET_H

#include <QWidget>
#include <QtCharts/QChartView>
#include <QLineSeries>
#include <QChart>
#include <QValueAxis>

namespace Ui { class DashboardWidget; }

class DashboardWidget : public QWidget {
    Q_OBJECT
public:
    explicit DashboardWidget(QWidget *parent = nullptr);
    ~DashboardWidget();
    void initChart(); // 차트를 초기화하는 함수

signals:
    void PageChangeCompLists(int index); // 페이지 전환 요청 시그널

private slots:
    void on_CompanyListBtn_clicked();

private:
    Ui::DashboardWidget *ui;

protected:
    // QWidget의 showEvent를 재정의합니다.
    void showEvent(QShowEvent *event) override;
};

#endif
