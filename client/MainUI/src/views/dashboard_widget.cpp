#include "dashboard_widget.h"
#include "ui_dashboard_widget.h"
#include <QVBoxLayout>

DashboardWidget::DashboardWidget(QWidget *parent) :
    QWidget(parent), ui(new Ui::DashboardWidget) {
    ui->setupUi(this);
}

DashboardWidget::~DashboardWidget() { delete ui; }

void DashboardWidget::initChart() {
    qDebug() << "Dashboard: initChart() called!";

    if (!ui->chartLayout) {
        qCritical() << "Dashboard Error: chartLayout is null!";
        return;
    }

    // 1. 기존 위젯 청소 (안전한 방식)
    QLayoutItem *child;
    while ((child = ui->chartLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }

    // 2. 차트 생성
    QLineSeries *series = new QLineSeries();
    series->append(0, 10); series->append(1, 25); series->append(2, 15);
    series->append(3, 30); series->append(4, 22); series->append(5, 40);

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("실시간 생산 현황 (MES 메인)");
    chart->createDefaultAxes();

    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    // 3. 레이아웃에 추가
    ui->chartLayout->addWidget(chartView);

    // 4. 강제 업데이트 호출
    this->update();
    qDebug() << "Dashboard: Chart added to layout successfully.";
}

void DashboardWidget::on_CompanyListBtn_clicked(){
    emit PageChangeCompLists(2);
}
