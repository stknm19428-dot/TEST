#include "dashboard_widget.h"
#include "ui_dashboard_widget.h"
#include "../core/user_session.h"
#include "../services/dashboard_service.h"

#include <QDebug>
#include <QObject>
#include <QLayoutItem>
#include <QLegendMarker>
#include <QGraphicsLayout>
#include <QMap>

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

    QString name = UserSession::instance().userName();
    if (!name.isEmpty()) {
        ui->userName->setText(name + "님 환영합니다.");
    } else {
        ui->userName->setText("로그인 정보 없음");
    }

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
    if (!ui->storageA01ChartLayout || !ui->storageB02ChartLayout || !ui->storageC03ChartLayout) {
        qCritical() << "Storage chart layouts are null";
        return;
    }

    auto chartDatas = DashboardService::getStorageCharts();
    auto locations  = DashboardService::getLocations();
    int totalSize = 200;

    clearLayout(ui->storageA01ChartLayout);
    clearLayout(ui->storageB02ChartLayout);
    clearLayout(ui->storageC03ChartLayout);

    for (const auto &loc : locations) {
        QPieSeries *series = new QPieSeries();
        int sumStock = 0;
        QStringList itemCodeList;

        for (const auto &data : chartDatas) {
            if (data.location == loc.location) {
                QPieSlice *slice = series->append(data.item_code, data.current_stock);
                slice->setLabel(QString::number(data.current_stock));
                slice->setLabelVisible(true);
                slice->setLabelPosition(QPieSlice::LabelInsideHorizontal);
                itemCodeList << data.item_code;

                QObject::connect(slice, &QPieSlice::hovered, this, [slice, data](bool hovered) {
                    slice->setExploded(hovered);
                    if (hovered) {
                        slice->setLabel(QString("%1 (%2)").arg(data.item_code).arg(data.current_stock));
                    } else {
                        slice->setLabel(QString::number(data.current_stock));
                    }
                });

                sumStock += data.current_stock;
            }
        }

        int remain = totalSize - sumStock;
        if (remain < 0) remain = 0;
        QPieSlice *remainSlice = series->append("여유 공간", remain);
        remainSlice->setLabel(remain > 0 ? "여유" : "");
        remainSlice->setLabelVisible(remain > 0);
        remainSlice->setBrush(QColor(80, 80, 80));
        itemCodeList << "여유 공간";

        QChart *chart = new QChart();
        chart->addSeries(series);
        chart->setTitle(loc.location);
        series->setPieSize(0.7);

        chart->legend()->setAlignment(Qt::AlignBottom);
        chart->legend()->setVisible(true);

        const auto markers = chart->legend()->markers(series);
        for (int i = 0; i < markers.count(); ++i) {
            if (i < itemCodeList.size()) {
                markers.at(i)->setLabel(itemCodeList.at(i));
            }
        }

        QFont legendFont = chart->legend()->font();
        legendFont.setPointSize(8);
        chart->legend()->setFont(legendFont);

        chart->setMargins(QMargins(0, 0, 0, 0));
        if (chart->layout()) chart->layout()->setContentsMargins(0, 0, 0, 0);

        QChartView *view = new QChartView(chart);
        view->setRenderHint(QPainter::Antialiasing);

        if (loc.location == "WH-A01") ui->storageA01ChartLayout->addWidget(view);
        else if (loc.location == "WH-B02") ui->storageB02ChartLayout->addWidget(view);
        else if (loc.location == "WH-C03") ui->storageC03ChartLayout->addWidget(view);
    }
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

    auto productionData = DashboardService::getProductionChart();

    QStringList dateList;
    QMap<QString, QBarSet*> barSetMap;

    for (const auto &data : productionData) {
        if (!dateList.contains(data.date)) {
            dateList.append(data.date);
        }
        if (!barSetMap.contains(data.product_name)) {
            barSetMap[data.product_name] = new QBarSet(data.product_name);
        }
    }

    // 날짜별로 제품 생산량 채우기 (없으면 0)
    for (const auto &productName : barSetMap.keys()) {
        for (const auto &date : dateList) {
            int count = 0;
            for (const auto &data : productionData) {
                if (data.product_name == productName && data.date == date) {
                    count = data.prod_count;
                    break;
                }
            }
            *barSetMap[productName] << count;
        }
    }

    QStackedBarSeries *series = new QStackedBarSeries();
    for (auto barSet : barSetMap.values()) {
        series->append(barSet);
    }

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Daily Production (2026-03-03 ~ 2026-03-09)");
    chart->setAnimationOptions(QChart::SeriesAnimations);

    // X축
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(dateList);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    // Y축 (타이틀 없음)
    QValueAxis *axisY = new QValueAxis();
    axisY->setLabelFormat("%d");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    // 범례
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignTop);

    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setMinimumHeight(300);

    ui->prodChartLayout->addWidget(chartView);  // ✅ 바로 추가

    qDebug() << "Production chart created, data count:" << productionData.size();
    qDebug() << "Dates:" << dateList;
    qDebug() << "Products:" << barSetMap.keys();
}


void DashboardWidget::on_CompanyListBtn_clicked()
{
    emit requestPageChange(PageType::PartnerManage);
}
void DashboardWidget::on_ScmManageBtn_clicked()
{
    emit requestPageChange(PageType::ScmManage);
}
void DashboardWidget::on_DeliveryBtn_clicked()
{
    emit requestPageChange(PageType::Delivery);
}
void DashboardWidget::on_ProcessBtn_clicked()
{
    emit requestPageChange(PageType::Process);
}
void DashboardWidget::on_ManufactureBtn_clicked()
{
    emit requestPageChange(PageType::Manufacture);
}
void DashboardWidget::on_ErrorLogBtn_clicked()
{
    emit requestPageChange(PageType::EnvironmentLogs);
}
