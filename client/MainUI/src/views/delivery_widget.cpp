#include "delivery_widget.h"
#include "ui_delivery_widget.h"
#include "../services/delivery_service.h"

DeliveryWidget::DeliveryWidget(QWidget *parent)
    : BasePageWidget(parent)
    , ui(new Ui::DeliveryWidget)
{
    ui->setupUi(this);
    ui->title_label->setText("Delivery List");

    setupTableConfigs();
    loadDeliveryData();
}

DeliveryWidget::~DeliveryWidget()
{
    delete ui;
}

void DeliveryWidget::setupTableConfigs()
{
    QStringList headers = {"Company", "Product", "Quantity", "Status", "Created at", "Updated at"};
    ui->delivery_table->setColumnCount(7); // 6 + 1 숨김(id)
    ui->delivery_table->setHorizontalHeaderLabels(headers);
    ui->delivery_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->delivery_table->setColumnHidden(6, true); // id 숨김

    ui->delivery_table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->delivery_table->verticalHeader()->setDefaultSectionSize(35);
    ui->delivery_table->verticalHeader()->setStretchLastSection(false);
}

void DeliveryWidget::loadDeliveryData()
{
    auto deliveries = DeliveryService::getDeliveries();

    ui->delivery_table->setRowCount(0);
    int row = 0;
    for (const auto &d : deliveries) {
        ui->delivery_table->insertRow(row);
        ui->delivery_table->setItem(row, 0, new QTableWidgetItem(d.company_name));
        ui->delivery_table->setItem(row, 1, new QTableWidgetItem(d.product_name));
        ui->delivery_table->setItem(row, 2, new QTableWidgetItem(QString::number(d.delivery_stock)));
        ui->delivery_table->setItem(row, 3, new QTableWidgetItem(d.status));
        ui->delivery_table->setItem(row, 4, new QTableWidgetItem(d.created_at));
        ui->delivery_table->setItem(row, 5, new QTableWidgetItem(d.updated_at));
        // 숨겨진 id 컬럼에 저장 (Complete 기능에 사용)
        ui->delivery_table->setItem(row, 6, new QTableWidgetItem(d.id));
        row++;
    }
    qDebug() << "총" << row << "개의 납품 항목을 불러왔습니다.";
}

void DeliveryWidget::on_create_delivery_button_clicked()
{
    // 납품 지시 팝업 호출
    DeliveryDialog *dialog = new DeliveryDialog(this);

    // 팝업 닫힌 후 테이블 새로고침
    if (dialog->exec() == QDialog::Accepted) {
        loadDeliveryData();
    }
}

void DeliveryWidget::on_complete_delivery_button_clicked()
{
    // 선택된 행 확인
    int row = ui->delivery_table->currentRow();
    if (row < 0) {
        qDebug() << "완료할 항목을 선택해주세요.";
        return;
    }

    // 이미 완료된 항목 확인
    QString status = ui->delivery_table->item(row, 3)->text();
    if (status == "DONE") {
        qDebug() << "이미 완료된 항목입니다.";
        return;
    }

    // 숨겨진 컬럼에서 id 가져오기
    QString selected_id = ui->delivery_table->item(row, 6)->text();

    if (DeliveryService::completeDelivery(selected_id)) {
        loadDeliveryData(); // 테이블 새로고침
    }
}
