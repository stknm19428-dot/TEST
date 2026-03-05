#include "dashboard_widget.h"
#include "ui_dashboard_widget.h"

#include <QDebug>
#include <QLayoutItem>

DashboardWidget::DashboardWidget(QWidget *parent)
    : BasePageWidget(parent),
      ui(new Ui::DashboardWidget)
{
    ui->setupUi(this);
}

DashboardWidget::~DashboardWidget() {
    delete ui;
}

void DashboardWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    initStorageCharts();
    initProductionChart();
}

void DashboardWidget::clearLayout(QLayout *layout)
{
    if (!layout) return;

    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
}







/* -----------------------------
   창고 Pie Chart
--------------------------------*/

void DashboardWidget::initStorageCharts()
{
    if (!ui->storageA01ChartLayout ||
        !ui->storageB02ChartLayout ||
        !ui->storageC03ChartLayout) {

        qCritical() << "Storage chart layouts are null";
        return;
    }

    clearLayout(ui->storageA01ChartLayout);
    clearLayout(ui->storageB02ChartLayout);
    clearLayout(ui->storageC03ChartLayout);


    /* ---------- 창고 A01 ---------- */

    QPieSeries *seriesA = new QPieSeries();
    seriesA->append("A-0 제품", 10);
    seriesA->append("A-1 제품", 60);
    seriesA->append("여유 공간", 30);

    QChart *chartA = new QChart();
    chartA->addSeries(seriesA);
    chartA->setTitle("Storage A01");

    QChartView *viewA = new QChartView(chartA);
    viewA->setRenderHint(QPainter::Antialiasing);

    ui->storageA01ChartLayout->addWidget(viewA);



    /* ---------- 창고 B02 ---------- */

    QPieSeries *seriesB = new QPieSeries();
    seriesB->append("B-0 제품", 70);
    seriesB->append("여유 공간", 30);

    QChart *chartB = new QChart();
    chartB->addSeries(seriesB);
    chartB->setTitle("Storage B02");

    QChartView *viewB = new QChartView(chartB);
    viewB->setRenderHint(QPainter::Antialiasing);

    ui->storageB02ChartLayout->addWidget(viewB);



    /* ---------- 창고 C03 ---------- */

    QPieSeries *seriesC = new QPieSeries();
    seriesC->append("C-0 제품", 55);
    seriesC->append("여유 공간", 45);

    QChart *chartC = new QChart();
    chartC->addSeries(seriesC);
    chartC->setTitle("Storage C03");

    QChartView *viewC = new QChartView(chartC);
    viewC->setRenderHint(QPainter::Antialiasing);

    ui->storageC03ChartLayout->addWidget(viewC);


    qDebug() << "Storage charts created";
}








/* -----------------------------
   생산량 Stacked Bar Chart
--------------------------------*/

void DashboardWidget::initProductionChart()
{
    if (!ui->prodChartLayout) {
        qCritical() << "prodChartLayout is null";
        return;
    }

    clearLayout(ui->prodChartLayout);



    /* 제품별 데이터 */

    QBarSet *productA = new QBarSet("Product A");
    QBarSet *productB = new QBarSet("Product B");
    QBarSet *productC = new QBarSet("Product C");

    *productA << 30 << 25 << 40 << 35 << 45 << 50 << 38;
    *productB << 20 << 18 << 22 << 25 << 28 << 30 << 26;
    *productC << 15 << 10 << 18 << 16 << 20 << 22 << 17;



    QStackedBarSeries *series = new QStackedBarSeries();
    series->append(productA);
    series->append(productB);
    series->append(productC);



    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Daily Production (2026-03-01 ~ 2026-03-07)");
    chart->setAnimationOptions(QChart::SeriesAnimations);



    /* X축 날짜 */

    QStringList categories;
    categories
        << "03-01"
        << "03-02"
        << "03-03"
        << "03-04"
        << "03-05"
        << "03-06"
        << "03-07";

    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);

    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);



    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);



    ui->prodChartLayout->addWidget(chartView);

    qDebug() << "Production chart created";
}






void DashboardWidget::on_CompanyListBtn_clicked()
{
    emit requestPageChange(PageType::PartnerManage);
}

void DashboardWidget::on_CompanyListBtn_clicked(){
    emit PageChangeCompLists(2);
}
