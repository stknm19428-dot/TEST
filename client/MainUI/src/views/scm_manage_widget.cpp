#include "scm_manage_widget.h"
#include "ui_scm_manage_widget.h"
#include "../services/scm_manage_service.h"
#include "order_edit_dialog.h"
#include "mainwindow.h"
#include "../services/opcua_service.h"

#include <QDebug>
#include <QHeaderView>
#include <QTimer>

ScmManageWidget::ScmManageWidget(QWidget *parent)
    : BasePageWidget(parent)
    , ui(new Ui::ScmManageWidget)
{
    ui->setupUi(this);

    setupStockStatusTableConfigs();
    setupStockOrderTableConfigs();
}

void ScmManageWidget::showEvent(QShowEvent *event)
{
    BasePageWidget::showEvent(event); // 부모 클래스 이벤트 처리

    // 1. 최신 DB 데이터 불러오기
    loadInventoryData();
    loadInventoryOrderData();

    // 2. OPC UA 바인딩 (이미 연결되었다면 m_opcBound에 의해 무시됨)
    QTimer::singleShot(0, this, [this](){
        setupOpcBindings();
    });
}

ScmManageWidget::~ScmManageWidget()
{
    delete ui;
}

void ScmManageWidget::setupStockStatusTableConfigs(){
    QStringList headers = {"Code", "Name", "Stock", "Location"};
    ui->tableStockStatus->setColumnCount(4);
    ui->tableStockStatus->setHorizontalHeaderLabels(headers);
    ui->tableStockStatus->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableStockStatus->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->tableStockStatus->verticalHeader()->setDefaultSectionSize(35);
    ui->tableStockStatus->verticalHeader()->setStretchLastSection(false);
}

void ScmManageWidget::setupStockOrderTableConfigs(){
    QStringList headers = {"User", "Item", "Stock", "Status", "Created at", "Receive at", "Updated at"};
    ui->tableStockOrder->setColumnCount(8);
    ui->tableStockOrder->setHorizontalHeaderLabels(headers);
    ui->tableStockOrder->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableStockOrder->setColumnHidden(7, true);
    ui->tableStockOrder->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->tableStockOrder->verticalHeader()->setDefaultSectionSize(35);
    ui->tableStockOrder->verticalHeader()->setStretchLastSection(false);
}

void ScmManageWidget::loadInventoryData(){
    auto inventoryList = ScmManageService::getInventoryStatus();

    ui->tableStockStatus->setRowCount(0);
    for (int i = 0; i < inventoryList.size(); ++i) {
        ui->tableStockStatus->insertRow(i);
        ui->tableStockStatus->setItem(i, 0, new QTableWidgetItem(inventoryList[i].item_code));
        ui->tableStockStatus->setItem(i, 1, new QTableWidgetItem(inventoryList[i].item_name));
        ui->tableStockStatus->setItem(i, 2, new QTableWidgetItem(QString::number(inventoryList[i].current_stock)));
        ui->tableStockStatus->setItem(i, 3, new QTableWidgetItem(inventoryList[i].location));
    }
}

void ScmManageWidget::loadInventoryOrderData(){
    auto orders = ScmManageService::getOrderLogs();

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
        ui->tableStockOrder->item(i, 0)->setData(Qt::UserRole, orders[i].id);
    }
}

void ScmManageWidget::setupOpcBindings()
{
    if (m_opcBound) return;

    auto *mw = qobject_cast<MainWindow*>(window());
    if (!mw) return;

    auto *ua = mw->opcUaService();
    if (!ua) return;

    m_opcBound = true;


    connect(ua, &OpcUaService::logArrivalRequested, this,
            [this, ua](const QString &orderId){
                qDebug() << "[SCM] arrival requested:" << orderId;

                InboundOrderTask task = ScmManageService::getInboundOrderTaskById(orderId);
                qDebug() << "[SCM] task.valid =" << task.valid
                         << "id =" << task.id
                         << "status =" << task.status
                         << "stock =" << task.stock
                         << "location =" << task.location
                         << "warehouseNo =" << task.warehouseNo;

                if (!task.valid) {
                    qDebug() << "[SCM] fail: order not found";
                    ua->logWriteArrivalResult(false, "order not found");
                    return;
                }

                if (task.status != "PENDING") {
                    qDebug() << "[SCM] fail: order is not PENDING, current =" << task.status;
                    ua->logWriteArrivalResult(false, "order is not PENDING");
                    return;
                }

                if (task.stock <= 0) {
                    qDebug() << "[SCM] fail: invalid stock =" << task.stock;
                    ua->logWriteArrivalResult(false, "invalid stock");
                    return;
                }

                if (task.warehouseNo < 1 || task.warehouseNo > 3) {
                    qDebug() << "[SCM] fail: invalid warehouse =" << task.warehouseNo
                             << "location =" << task.location;
                    ua->logWriteArrivalResult(false, "invalid warehouse");
                    return;
                }

                if (m_activeOrderIdByWh.contains(task.warehouseNo)) {
                    qDebug() << "[SCM] fail: warehouse busy =" << task.warehouseNo
                             << "active order =" << m_activeOrderIdByWh.value(task.warehouseNo);
                    ua->logWriteArrivalResult(false, "warehouse busy");
                    return;
                }

                bool ok = ScmManageService::markOrderInProc(task.id);
                qDebug() << "[SCM] markOrderInProc =" << ok;

                if (!ok) {
                    qDebug() << "[SCM] fail: failed to mark INPROC";
                    ua->logWriteArrivalResult(false, "failed to mark INPROC");
                    return;
                }
                m_activeOrderIdByWh[task.warehouseNo] = task.id;

                qDebug() << "[SCM] call logMove wh =" << task.warehouseNo << "qty =" << task.stock;
                ua->logMove(task.warehouseNo, static_cast<quint32>(task.stock));
                ua->logWriteArrivalResult(true, "accepted");

                loadInventoryOrderData();
            });

    connect(ua, &OpcUaService::logWhQtyUpdated, this,
            [this](int wh, quint32 qty){
                quint32 prev = m_lastQtyByWh.value(wh, qty);
                m_lastQtyByWh[wh] = qty;

                if (!m_activeOrderIdByWh.contains(wh))
                    return;

                if (qty <= prev)
                    return;

                int delta = static_cast<int>(qty - prev);
                QString orderId = m_activeOrderIdByWh.value(wh);

                if (ScmManageService::increaseInventoryByOrderId(orderId, delta)) {
                    qDebug() << "[SCM] WH" << wh << "inventory +" << delta;
                    loadInventoryData();
                    loadInventoryOrderData();
                }
            });

    connect(ua, &OpcUaService::logWhLoadedUpdated, this,
            [this](int wh, bool loaded){
                if (!loaded) return;
                if (!m_activeOrderIdByWh.contains(wh)) return;

                QString orderId = m_activeOrderIdByWh.value(wh);

                qDebug() << "[SCM] WH" << wh << "loaded=true, delay DONE";

                QTimer::singleShot(300, this, [this, wh, orderId]() {
                    if (!m_activeOrderIdByWh.contains(wh))
                        return;
                    if (m_activeOrderIdByWh.value(wh) != orderId)
                        return;

                    m_activeOrderIdByWh.remove(wh);
                    ScmManageService::markOrderDone(orderId);

                    qDebug() << "[SCM] WH" << wh << "DONE:" << orderId;
                    loadInventoryOrderData();
                });
            });
}

void ScmManageWidget::on_cancel_order_button_clicked()
{
    int row = ui->tableStockOrder->currentRow();
    if (row < 0) return;

    QString status = ui->tableStockOrder->item(row, 3)->text();
    if (status == "DONE" || status == "INPROC") {
        qDebug() << "취소할 수 없는 상태입니다.";
        return;
    }

    QString selected_id = ui->tableStockOrder->item(row, 0)->data(Qt::UserRole).toString();

    if (ScmManageService::cancelOrder(selected_id)) {
        qDebug() << "취소 성공";
        loadInventoryOrderData();
    }
}

void ScmManageWidget::on_pushButton_clicked()
{
    OrderEditDialog dialog(this);
    dialog.loadInventoryList();

    if (dialog.exec() == QDialog::Accepted) {
        QString selectedDate = dialog.getDueDateTime();
        bool success = ScmManageService::addOrder(
            "userName",
            dialog.getSelectedItemCode(),
            dialog.getOrderAmount(),
            selectedDate
            );
        if (success) {
            loadInventoryOrderData();
        }
    }
}

void ScmManageWidget::on_Back_btn_clicked()
{
    emit requestPageChange(PageType::Dashboard);
}
