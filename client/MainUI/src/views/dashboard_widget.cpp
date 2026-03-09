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
#include <QLabel>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QTimer>

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

    initSensorWidget();
    initConveyorWidget();
    initStorageCharts();
    initProductionChart();

    // 테스트용 - 나중에 삭제
    QTimer::singleShot(2000, this, [this]() {
        update_log_conveyor(1, true);   // 컨베이어1 초록
        update_log_conveyor(2, false);  // 컨베이어2 빨강
        update_log_conveyor(3, true);   // 컨베이어3 초록
    });
}

void DashboardWidget::set_opcua_service(OpcUaService *service)
{
    m_opcua_service = service;
    if (!m_opcua_service) return;

    connect(m_opcua_service, &OpcUaService::mfgTempUpdated,
            this, &DashboardWidget::update_mfg_temp);
    connect(m_opcua_service, &OpcUaService::mfgHumUpdated,
            this, &DashboardWidget::update_mfg_hum);
    connect(m_opcua_service, &OpcUaService::logTempUpdated,
            this, &DashboardWidget::update_log_temp);
    connect(m_opcua_service, &OpcUaService::logHumUpdated,
            this, &DashboardWidget::update_log_hum);
    connect(m_opcua_service, &OpcUaService::logWhLoadingUpdated,
            this, &DashboardWidget::update_log_conveyor);
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
   센서 온습도 표
--------------------------------*/

void DashboardWidget::initSensorWidget()
{
    if (!ui->sensor_widget) return;
    if (ui->sensor_widget->layout()) return;

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(6);
    grid->setContentsMargins(4, 4, 4, 4);

    auto make_header = [](const QString &text) {
        QLabel *lbl = new QLabel(text);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(
            "background-color: #3a3a3a;"
            "color: white;"
            "font-weight: bold;"
            "font-size: 11px;"
            "padding: 5px;"
            "border-radius: 4px;"
            );
        return lbl;
    };

    auto make_value = [](const QString &text) {
        QLabel *lbl = new QLabel(text);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(
            "background-color: #f0f4ff;"
            "color: #222;"
            "font-size: 11px;"
            "padding: 5px;"
            "border: 1px solid #ccc;"
            "border-radius: 4px;"
            );
        return lbl;
    };

    // 1행: 제조 온도 | 값 | 제조 습도 | 값
    grid->addWidget(make_header("제조 온도"), 0, 0);
    mfg_temp_value = make_value("-- °C");
    grid->addWidget(mfg_temp_value, 0, 1);

    grid->addWidget(make_header("제조 습도"), 0, 2);
    mfg_hum_value = make_value("-- %");
    grid->addWidget(mfg_hum_value, 0, 3);

    // 2행: 물류 온도 | 값 | 물류 습도 | 값
    grid->addWidget(make_header("물류 온도"), 1, 0);
    log_temp_value = make_value("-- °C");
    grid->addWidget(log_temp_value, 1, 1);

    grid->addWidget(make_header("물류 습도"), 1, 2);
    log_hum_value = make_value("-- %");
    grid->addWidget(log_hum_value, 1, 3);

    ui->sensor_widget->setLayout(grid);
}

void DashboardWidget::update_mfg_temp(double temp)
{
    if (mfg_temp_value)
        mfg_temp_value->setText(QString::number(temp, 'f', 1) + " °C");
}

void DashboardWidget::update_mfg_hum(double hum)
{
    if (mfg_hum_value)
        mfg_hum_value->setText(QString::number(hum, 'f', 1) + " %");
}

void DashboardWidget::update_log_temp(double temp)
{
    if (log_temp_value)
        log_temp_value->setText(QString::number(temp, 'f', 1) + " °C");
}

void DashboardWidget::update_log_hum(double hum)
{
    if (log_hum_value)
        log_hum_value->setText(QString::number(hum, 'f', 1) + " %");
}


/* -----------------------------
   물류 컨베이어 가동 현황
--------------------------------*/

void DashboardWidget::initConveyorWidget()
{
    if (!ui->conveyor_widget) return;
    if (ui->conveyor_widget->layout()) return;

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(8);
    grid->setContentsMargins(4, 4, 4, 4);

    QStringList names = {"컨베이어1", "컨베이어2", "컨베이어3"};

    for (int i = 0; i < 3; ++i) {
        // 이름 라벨
        QLabel *name_label = new QLabel(names[i]);
        name_label->setAlignment(Qt::AlignCenter);
        name_label->setStyleSheet(
            "background-color: #3a3a3a;"
            "color: white;"
            "font-weight: bold;"
            "font-size: 11px;"
            "padding: 5px;"
            "border-radius: 4px;"
            );
        grid->addWidget(name_label, 0, i);

        // 상태 원 라벨
        QLabel *status_label = new QLabel();
        status_label->setAlignment(Qt::AlignCenter);
        status_label->setFixedSize(40, 40);
        status_label->setStyleSheet(
            "background-color: #e74c3c;"  // 기본 빨간원
            "border-radius: 20px;"
            );
        grid->addWidget(status_label, 1, i, Qt::AlignHCenter);
        conveyor_status[i] = status_label;
    }

    ui->conveyor_widget->setLayout(grid);
}

void DashboardWidget::update_log_conveyor(int idx, bool loading)
{
    int i = idx - 1;  // 1~3 -> 0~2
    if (i < 0 || i > 2) return;
    if (!conveyor_status[i]) return;

    if (loading) {
        conveyor_status[i]->setStyleSheet(
            "background-color: #2ecc71;"  // 초록원
            "border-radius: 20px;"
            );
    } else {
        conveyor_status[i]->setStyleSheet(
            "background-color: #e74c3c;"  // 빨간원
            "border-radius: 20px;"
            );
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

#include "dashboard_widget.moc"
