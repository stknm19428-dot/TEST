#include "manufacture_widget.h"
#include "ui_manufacture_widget.h"
#include "manufacture_schedule_dialog.h"
#include "../services/manufacture_service.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

ManufactureWidget::ManufactureWidget(QWidget *parent)
    : BasePageWidget(parent)
    , ui(new Ui::ManufactureWidget)
{
    ui->setupUi(this);

    setupManufactureTableConfigs();
    setupScheduleTableConfigs();
    loadManufactureData();
    loadScheduleData();
}

ManufactureWidget::~ManufactureWidget()
{
    delete ui;
}

QTableWidgetItem* createManufactureCenteredItem(const QString &text) {
    QTableWidgetItem *item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

void ManufactureWidget::setupManufactureTableConfigs()
{
    QStringList headers = {"Code", "Name", "Stock", "Description"};
    ui->manufacture_table->setColumnCount(5); // 4 + 1 숨김(id)
    ui->manufacture_table->setHorizontalHeaderLabels(headers);
    ui->manufacture_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->manufacture_table->setColumnHidden(4, true); // id 숨김

    // 세로 스크롤바가 필요할 때만 나타나도록 설정
    ui->manufacture_table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // 마우스 휠 한 번에 이동하는 픽셀 수 조절 (부드러운 스크롤)
    ui->manufacture_table->verticalHeader()->setDefaultSectionSize(35); // 행 높이 고정

    // 테이블 끝에 도달했을 때 빈 공간이 생기지 않도록 설정
    ui->manufacture_table->verticalHeader()->setStretchLastSection(false);
}

void ManufactureWidget::setupScheduleTableConfigs()
{
    QStringList headers = {"Product", "Order Count", "Motor Speed", "Status", "Created at", "Deadline at", "Updated at"};
    ui->schedule_table->setColumnCount(8); // 7 + 1 숨김(id)
    ui->schedule_table->setHorizontalHeaderLabels(headers);
    ui->schedule_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->schedule_table->setColumnHidden(7, true); // id 숨김

    // 세로 스크롤바가 필요할 때만 나타나도록 설정
    ui->schedule_table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // 마우스 휠 한 번에 이동하는 픽셀 수 조절 (부드러운 스크롤)
    ui->schedule_table->verticalHeader()->setDefaultSectionSize(35); // 행 높이 고정

    // 테이블 끝에 도달했을 때 빈 공간이 생기지 않도록 설정
    ui->schedule_table->verticalHeader()->setStretchLastSection(false);
}

void ManufactureWidget::loadManufactureData()
{
    auto products = ManufactureService::getProducts();

    product_id_list.clear();
    ui->manufacture_table->setRowCount(0);

    for (int i = 0; i < products.size(); ++i) {
        ui->manufacture_table->insertRow(i);
        ui->manufacture_table->setItem(i, 0, new QTableWidgetItem(products[i].code));
        ui->manufacture_table->setItem(i, 1, new QTableWidgetItem(products[i].name));
        ui->manufacture_table->setItem(i, 2, new QTableWidgetItem(QString::number(products[i].stock)));
        ui->manufacture_table->setItem(i, 3, new QTableWidgetItem(products[i].description));
        ui->manufacture_table->setItem(i, 4, new QTableWidgetItem(products[i].id)); // 숨김 id
        product_id_list.append(products[i].id);
    }
    qDebug() << "총" << products.size() << "개의 제품을 불러왔습니다.";
}

void ManufactureWidget::loadScheduleData()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "Manufacture: 데이터베이스가 열려있지 않습니다.";
        return;
    }

    // 최신순 정렬
    QSqlQuery query(
        "SELECT p.product_code, p.product_name, o.id, o.order_count, o.motor_speed, "
        "o.status, o.created_at, o.deadline_at, o.updated_at "
        "FROM product_order_logs o "
        "LEFT JOIN product p ON o.product_id = p.id "
        "ORDER BY o.created_at DESC", db);

    if (!query.exec()) {
        qDebug() << "스케줄 조회 에러:" << query.lastError().text();
        return;
    }

    ui->schedule_table->setRowCount(0);
    int row = 0;
    while (query.next()) {
        ui->schedule_table->insertRow(row);
        ui->schedule_table->setItem(row, 0, createManufactureCenteredItem(
                                                QString("[%1] %2").arg(query.value("product_code").toString(), query.value("product_name").toString())
                                                ));
        ui->schedule_table->setItem(row, 1, createManufactureCenteredItem(query.value("order_count").toString()));
        ui->schedule_table->setItem(row, 2, createManufactureCenteredItem(query.value("motor_speed").toString()));
        ui->schedule_table->setItem(row, 3, createManufactureCenteredItem(query.value("status").toString()));
        ui->schedule_table->setItem(row, 4, new QTableWidgetItem(query.value("created_at").toString()));
        ui->schedule_table->setItem(row, 5, new QTableWidgetItem(query.value("deadline_at").toString()));
        ui->schedule_table->setItem(row, 6, new QTableWidgetItem(query.value("updated_at").toString()));
        // 숨겨진 id 컬럼에 저장 (Cancel Schedule 기능에 사용)
        ui->schedule_table->setItem(row, 7, new QTableWidgetItem(query.value("id").toString()));
        row++;
    }
    qDebug() << "총" << row << "개의 스케줄을 불러왔습니다.";
}

void ManufactureWidget::on_edit_stock_button_clicked()
{
    // 선택된 행 확인
    int row = ui->manufacture_table->currentRow();
    if (row < 0) {
        qDebug() << "수정할 항목을 선택해주세요.";
        return;
    }

    QString product_name = ui->manufacture_table->item(row, 1)->text();
    int current_stock    = ui->manufacture_table->item(row, 2)->text().toInt();
    QString product_id   = product_id_list[row];

    // 입력 다이얼로그
    bool ok;
    int new_stock = QInputDialog::getInt(
        this,
        "Edit Stock",
        product_name + " 재고 수정:",
        current_stock, // 기본값
        0,             // 최솟값
        99999,         // 최댓값
        1,
        &ok
        );

    if (ok) {
        if (ManufactureService::updateProductStock(product_id, new_stock)) {
            loadManufactureData(); // 테이블 새로고침
        }
    }
}

void ManufactureWidget::on_create_schedule_button_clicked()
{
    // 제조 스케줄 생성 팝업 호출
    ManufactureScheduleDialog *dialog = new ManufactureScheduleDialog(this);
    if (dialog->exec() == QDialog::Accepted) {
        loadScheduleData(); // 테이블 새로고침
    }
}

void ManufactureWidget::on_cancel_schedule_button_clicked()
{
    // 선택된 행 확인
    int row = ui->schedule_table->currentRow();
    if (row < 0) {
        qDebug() << "취소할 항목을 선택해주세요.";
        return;
    }

    // 완료된 항목은 취소 불가
    QString status = ui->schedule_table->item(row, 3)->text();
    if (status == "DONE") {
        qDebug() << "이미 완료된 항목은 취소할 수 없습니다.";
        return;
    }

    // 숨겨진 컬럼에서 id 가져오기
    QString selected_id = ui->schedule_table->item(row, 7)->text();

    // DB 연결 확인
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "DB 연결이 열려있지 않습니다.";
        return;
    }

    // DB에서 해당 row 삭제
    QSqlQuery query(db);
    query.prepare("DELETE FROM product_order_logs WHERE id = :id");
    query.bindValue(":id", selected_id);

    if (query.exec()) {
        qDebug() << "[스케줄 취소 성공]" << selected_id;
        loadScheduleData(); // 테이블 새로고침
    } else {
        qDebug() << "[스케줄 취소 실패]" << query.lastError().text();
    }
}

void ManufactureWidget::on_Back_btn_clicked()
{
    emit requestPageChange(PageType::Dashboard);
}
