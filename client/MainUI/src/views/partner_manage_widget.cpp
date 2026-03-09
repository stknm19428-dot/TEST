#include "partner_manage_widget.h"
#include "ui_partner_manage_widget.h"
#include "../services/partner_manage_service.h"
#include "../core/database_manager.h"
#include <QSqlQuery>               //
#include <QSqlError>
#include <QDebug>

PartnerManageWidget::PartnerManageWidget(QWidget *parent) :
    BasePageWidget(parent),
    ui(new Ui::PartnerManageWidget)
{
    ui->setupUi(this);

    setupTableConfigs();
    loadAllData();
}

void PartnerManageWidget::showEvent(QShowEvent *event)
{
    BasePageWidget::showEvent(event);
    loadAllData(); // 페이지 진입 시마다 협력사/고객사 명단 새로고침
}

void PartnerManageWidget::setupTableConfigs(){
    QStringList headers = {"name","address","contact"};

    ui->tableSuppCompany->setColumnCount(3);
    ui->tableSuppCompany->setHorizontalHeaderLabels(headers);
    ui->tableSuppCompany->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    ui->tableCustCompany->setColumnCount(3);
    ui->tableCustCompany->setHorizontalHeaderLabels(headers);
    ui->tableCustCompany->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}
void PartnerManageWidget::loadAllData() {
    // 1. 서비스에 데이터 요청 (API 호출과 유사)
    auto suppliers = PartnerService::getSuppliers();
    auto customers = PartnerService::getCustomers();

    // 2. 결과 출력 (Supplier 테이블)
    ui->tableSuppCompany->setRowCount(0);
    for (int i = 0; i < suppliers.size(); ++i) {
        ui->tableSuppCompany->insertRow(i);
        ui->tableSuppCompany->setItem(i, 0, new QTableWidgetItem(suppliers[i].name));
        ui->tableSuppCompany->setItem(i, 1, new QTableWidgetItem(suppliers[i].address));
        ui->tableSuppCompany->setItem(i, 2, new QTableWidgetItem(suppliers[i].contact));
        ui->tableSuppCompany->item(i, 0)->setData(Qt::UserRole, suppliers[i].id);
    }
    
    // Customer 테이블도 동일하게 처리...
    ui->tableCustCompany->setRowCount(0);
    for (int i = 0; i < customers.size(); ++i) {
        ui->tableCustCompany->insertRow(i);
        ui->tableCustCompany->setItem(i, 0, new QTableWidgetItem(customers[i].name));
        ui->tableCustCompany->setItem(i, 1, new QTableWidgetItem(customers[i].address));
        ui->tableCustCompany->setItem(i, 2, new QTableWidgetItem(customers[i].contact));
        ui->tableCustCompany->item(i, 0)->setData(Qt::UserRole, customers[i].id);
    }
}

PartnerManageWidget::~PartnerManageWidget()
{
    delete ui;
}

void PartnerManageWidget::on_Back_btn_clicked()
{
    emit requestPageChange(PageType::Dashboard);
}

