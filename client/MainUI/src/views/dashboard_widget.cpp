#include "dashboard_widget.h"
#include "ui_dashboard_widget.h"
#include "../core/user_session.h"
#include "../services/dashboard_service.h"

#include <QDebug>
#include <QObject>
#include <QLayoutItem>
#include <QLegendMarker>
#include <QGraphicsLayout>

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
        QStringList itemCodeList; // 범례 텍스트를 순서대로 담아둘 리스트

        for (const auto &data : chartDatas) {
            if (data.location == loc.location) {
                // 1. 슬라이스 추가 (범례에 표시될 데이터 연결)
                QPieSlice *slice = series->append(data.item_code, data.current_stock);
                
                // 2. [수정] 그래프 위 레이블은 숫자만 표시
                slice->setLabel(QString::number(data.current_stock)); 
                slice->setLabelVisible(true);
                slice->setLabelPosition(QPieSlice::LabelInsideHorizontal);
                
                // 범례용 품목코드 저장
                itemCodeList << data.item_code;

                // 마우스 호버 시 효과: 품목코드와 숫자를 동시에 보여줌
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

        // 여유 공간 처리
        int remain = totalSize - sumStock;
        if (remain < 0) remain = 0;
        QPieSlice *remainSlice = series->append("여유 공간", remain);
        remainSlice->setLabel(remain > 0 ? "여유" : "");
        remainSlice->setLabelVisible(remain > 0);
        remainSlice->setBrush(QColor(80, 80, 80)); 
        itemCodeList << "여유 공간";

        /* ---------- 차트 생성 및 범례 강제 설정 ---------- */
        QChart *chart = new QChart();
        chart->addSeries(series);
        chart->setTitle(loc.location);
        series->setPieSize(0.7);

        chart->legend()->setAlignment(Qt::AlignBottom);
        chart->legend()->setVisible(true);

        // [핵심] 차트가 그려진 후 마커의 텍스트를 item_code로 강제 고정
        // markers() 함수는 차트가 생성된 직후에 호출해야 합니다.
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
