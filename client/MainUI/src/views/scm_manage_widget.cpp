#include "scm_manage_widget.h"
#include "ui_scm_manage_widget.h"
#include "order_edit_dialog.h"
#include "scm_manage_service.h"
#include <QSqlQuery>               //
#include <QSqlError>
#include <QDebug>

ScmManageWidget::ScmManageWidget(QWidget *parent) :
    BasePageWidget(parent),
    ui(new Ui::ScmManageWidget)
{
    ui->setupUi(this);

    setupStockStatusTableConfigs();
    setupStockOrderTableConfigs();
    loadInventoryData();
    loadInventoryOrderData();
}

QTableWidgetItem* createCenteredItem(const QString &text) {
    QTableWidgetItem *item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter); // 💡 핵심: 가운데 정렬 설정
    return item;
}

void ScmManageWidget::setupStockStatusTableConfigs(){
    // 1. UI 구조 설정
    QStringList headers = {"Code", "Name", "Stock", "Location"};
    ui->tableStockStatus->setColumnCount(headers.size());
    ui->tableStockStatus->setHorizontalHeaderLabels(headers);
    ui->tableStockStatus->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // 세로 스크롤바가 필요할 때만 나타나도록 설정
    ui->tableStockOrder->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // 마우스 휠 한 번에 이동하는 픽셀 수 조절 (부드러운 스크롤)
    ui->tableStockOrder->verticalHeader()->setDefaultSectionSize(35); // 행 높이 고정

    // 테이블 끝에 도달했을 때 빈 공간이 생기지 않도록 설정
    ui->tableStockOrder->verticalHeader()->setStretchLastSection(false);
}

void ScmManageWidget::setupStockOrderTableConfigs(){
    QStringList headers = {"User", "Item", "Stock", "Status", "Created at","Receive at", "Updated at"};
    ui->tableStockOrder->setColumnCount(8); // 7 → 8로 변경 (id 숨김 컬럼 추가)
    ui->tableStockOrder->setHorizontalHeaderLabels(headers);
    ui->tableStockOrder->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); //
    ui->tableStockOrder->setColumnHidden(7, true); // id 컬럼 숨김 처리

    // 세로 스크롤바가 필요할 때만 나타나도록 설정
    ui->tableStockOrder->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // 마우스 휠 한 번에 이동하는 픽셀 수 조절 (부드러운 스크롤)
    ui->tableStockOrder->verticalHeader()->setDefaultSectionSize(35); // 행 높이 고정
    // 테이블 끝에 도달했을 때 빈 공간이 생기지 않도록 설정
    ui->tableStockOrder->verticalHeader()->setStretchLastSection(false);
}

void ScmManageWidget::loadInventoryData(){
    // 서비스를 통해 데이터 획득
    auto inventoryList = ScmManageService::getInventoryStatus();

    ui->tableStockStatus->setRowCount(0);
    for (int i = 0; i < inventoryList.size(); ++i) {
        ui->tableStockStatus->insertRow(i);
        // 가운데 정렬을 위해 이전에 만든 createCenteredItem 활용 권장
        ui->tableStockStatus->setItem(i, 0, new QTableWidgetItem(inventoryList[i].item_code));
        ui->tableStockStatus->setItem(i, 1, new QTableWidgetItem(inventoryList[i].item_name));
        ui->tableStockStatus->setItem(i, 2, new QTableWidgetItem(QString::number(inventoryList[i].current_stock)));
        ui->tableStockStatus->setItem(i, 3, new QTableWidgetItem(inventoryList[i].location));
    }
}

void ScmManageWidget::loadInventoryOrderData(){
    // 1. 서비스에 데이터 요청
    auto orders = ScmManageService::getOrderLogs();

    // 2. 테이블 뷰 업데이트 (UI 작업에만 집중)
    ui->tableStockOrder->setRowCount(0);
    for (int i = 0; i < orders.size(); ++i) {
        ui->tableStockOrder->insertRow(i);
        ui->tableStockOrder->setItem(i, 0, new QTableWidgetItem(orders[i].userName));
        ui->tableStockOrder->setItem(i, 1, new QTableWidgetItem(orders[i].itemName));
        ui->tableStockOrder->setItem(i, 2, new QTableWidgetItem(QString::number(orders[i].stock)));
        ui->tableStockOrder->setItem(i, 3, new QTableWidgetItem(orders[i].status));
        ui->tableStockOrder->setItem(i, 4, new QTableWidgetItem(orders[i].createdAt));
        ui->tableStockOrder->setItem(i, 5, new QTableWidgetItem(orders[i].receiveAt));
        ui->tableStockOrder->setItem(i, 6, new QTableWidgetItem(orders[i].updatedAt));
        // 행에 ID 데이터를 숨겨
        ui->tableStockOrder->item(i, 0)->setData(Qt::UserRole, orders[i].id);
    }
}
void ScmManageWidget::on_create_order_button_clicked()
{
    // 입고 스케줄 생성 팝업 호출
    LogisticsScheduleDialog *dialog = new LogisticsScheduleDialog(this);

    // 팝업 닫힌 후 테이블 새로고침
    if (dialog->exec() == QDialog::Accepted) {
        loadInventoryOrderData();
    }
}

void ScmManageWidget::on_cancel_order_button_clicked()
{
    // 선택된 행 확인
    int row = ui->tableStockOrder->currentRow();
    if (row < 0) {
        qDebug() << "취소할 항목을 선택해주세요.";
        return;
    }

    // 완료된 항목은 취소 불가
    QString status = ui->tableStockOrder->item(row, 3)->text();
    if (status == "DONE") {
        qDebug() << "이미 완료된 항목은 취소할 수 없습니다.";
        return;
    }

    // 숨겨진 컬럼에서 id 가져오기
    QString selected_id = ui->tableStockOrder->item(row, 7)->text();

    // DB 연결 확인
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "DB 연결이 열려있지 않습니다.";
        return;
    }

    // DB에서 해당 row 삭제
    QSqlQuery query(db);
    query.prepare("DELETE FROM inventory_order_logs WHERE id = :id");
    query.bindValue(":id", selected_id);

    if (query.exec()) {
        qDebug() << "[스케줄 취소 성공]" << selected_id;
        loadInventoryOrderData(); // 테이블 새로고침
    } else {
        qDebug() << "[스케줄 취소 실패]" << query.lastError().text();
    }
}

ScmManageWidget::~ScmManageWidget()
{
    delete ui;
}

void ScmManageWidget::on_pushButton_clicked()
{
    OrderEditDialog dialog(this);
    dialog.loadInventoryList();

    if (dialog.exec() == QDialog::Accepted) {
        // 서비스 호출로 주문 추가
        bool success = ScmManageService::addOrder("userName",
                                            dialog.getSelectedItemCode(),
                                            dialog.getOrderAmount());
        if (success) {
            loadInventoryOrderData();
        }
    }
}

