#include "delivery_widget.h"
#include "ui_delivery_widget.h"

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
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "Delivery: 데이터베이스가 열려있지 않습니다.";
        return;
    }

    // 최신순 정렬
    QSqlQuery query(
        "SELECT d.id, c.company_name, p.product_name, d.delivery_stock, "
        "d.status, d.created_at, d.updated_at "
        "FROM product_deli_logs d "
        "LEFT JOIN cust_company c ON d.company_id = c.id "
        "LEFT JOIN product p ON d.product_id = p.id "
        "ORDER BY d.created_at DESC", db);

    if (!query.exec()) {
        qDebug() << "납품 조회 에러:" << query.lastError().text();
        return;
    }

    ui->delivery_table->setRowCount(0);
    int row = 0;
    while (query.next()) {
        ui->delivery_table->insertRow(row);
        ui->delivery_table->setItem(row, 0, new QTableWidgetItem(query.value("company_name").toString()));
        ui->delivery_table->setItem(row, 1, new QTableWidgetItem(query.value("product_name").toString()));
        ui->delivery_table->setItem(row, 2, new QTableWidgetItem(query.value("delivery_stock").toString()));
        ui->delivery_table->setItem(row, 3, new QTableWidgetItem(query.value("status").toString()));
        ui->delivery_table->setItem(row, 4, new QTableWidgetItem(query.value("created_at").toString()));
        ui->delivery_table->setItem(row, 5, new QTableWidgetItem(query.value("updated_at").toString()));
        // 숨겨진 id 컬럼에 저장 (Complete 기능에 사용)
        ui->delivery_table->setItem(row, 6, new QTableWidgetItem(query.value("id").toString()));
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

    // DB 연결 확인
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "DB 연결이 열려있지 않습니다.";
        return;
    }

    // status를 DONE으로 업데이트
    QSqlQuery query(db);
    query.prepare("UPDATE product_deli_logs SET status = 'DONE', updated_at = NOW() WHERE id = :id");
    query.bindValue(":id", selected_id);

    if (query.exec()) {
        qDebug() << "[납품 완료 처리 성공]" << selected_id;
        loadDeliveryData(); // 테이블 새로고침
    } else {
        qDebug() << "[납품 완료 처리 실패]" << query.lastError().text();
    }
}
