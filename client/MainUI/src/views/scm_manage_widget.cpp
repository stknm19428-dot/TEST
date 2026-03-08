#include "scm_manage_widget.h"
#include "ui_scm_manage_widget.h"
#include "user_session.h"
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

void ScmManageWidget::on_cancel_order_button_clicked()
{
    int row = ui->tableStockOrder->currentRow();
    if (row < 0) return;

    // 1. 상태 확인 (3번 컬럼: Status)
    QString status = ui->tableStockOrder->item(row, 3)->text();
    if (status == "DONE" || status == "INPROC") { // 진행 중이거나 완료된 것은 취소 불가
        qDebug() << "취소할 수 없는 상태입니다.";
        return;
    }

    // 2. 0번 컬럼에 숨겨둔 UserRole 데이터(ID) 가져오기
    QString selected_id = ui->tableStockOrder->item(row, 0)->data(Qt::UserRole).toString();

    // 3. 서비스 호출 (직접 쿼리 대신)
    if (ScmManageService::cancelOrder(selected_id)) {
        qDebug() << "취소 성공";
        loadInventoryOrderData(); // 새로고침
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


        QString selectedDate = dialog.getDueDateTime();
        bool success = ScmManageService::addOrder("userName",
                                            dialog.getSelectedItemCode(),
                                            dialog.getOrderAmount(),
                                            selectedDate);
        if (success) {
            loadInventoryOrderData();
        }
    }
}

void ScmManageWidget::on_Back_btn_clicked()
{
    emit requestPageChange(PageType::Dashboard);
}

