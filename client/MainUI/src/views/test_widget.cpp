#include "test_widget.h"
#include "ui_test_widget.h"
#include "../core/database_manager.h"
#include <QSqlQuery>               //
#include <QSqlError>
#include <QDebug>

TestWidget::TestWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TestWidget)
{
    ui->setupUi(this);

    //setupTableConfigs();
    //loadAllData();
}

// void TestWidget::setupTableConfigs(){
//     QStringList headers = {"name","address","contact"};

//     ui->tableSuppCompany->setColumnCount(3);
//     ui->tableSuppCompany->setHorizontalHeaderLabels(headers);
//     ui->tableSuppCompany->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

//     ui->tableCustCompany->setColumnCount(3);
//     ui->tableCustCompany->setHorizontalHeaderLabels(headers);
//     ui->tableCustCompany->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
// }

// void TestWidget::loadAllData(){
//     if(!DatabaseManager::instance().connect()){
//         qDebug() << "Database connect failed!";
//         return;
//     }

//     QSqlQuery query;
//     // 1. 쿼리는 그대로 유지 (나중에 삭제/수정 기능을 위해 id가 필요함)
//     query.prepare("SELECT id, company_name, company_address, company_number FROM supp_company");

//     if (query.exec()) {
//         ui->tableSuppCompany->setRowCount(0);
//         int row = 0;
//         while (query.next()) {
//             ui->tableSuppCompany->insertRow(row);

//             // 2. UI에는 UUID를 제외하고 name, address, contact 순서로 매핑
//             // query.value(0)은 'id'이므로 건너뛰고 1번부터 사용합니다.
//             ui->tableSuppCompany->setItem(row, 0, new QTableWidgetItem(query.value("company_name").toString()));
//             ui->tableSuppCompany->setItem(row, 1, new QTableWidgetItem(query.value("company_address").toString()));
//             ui->tableSuppCompany->setItem(row, 2, new QTableWidgetItem(query.value("company_number").toString()));

//             // 팁: UUID를 숨겨두고 싶다면 'id'를 데이터 역할(UserRole)로 저장해둘 수 있습니다.
//             ui->tableSuppCompany->item(row, 0)->setData(Qt::UserRole, query.value("id").toString());

//             row++;
//         }
//     }

//     if (query.exec("SELECT id, company_name, company_address, company_number FROM cust_company")) {
//         ui->tableCustCompany->setRowCount(0);
//         int row = 0;
//         while (query.next()) {
//             ui->tableCustCompany->insertRow(row);
//             ui->tableCustCompany->setItem(row, 0, new QTableWidgetItem(query.value("company_name").toString()));
//             ui->tableCustCompany->setItem(row, 1, new QTableWidgetItem(query.value("company_address").toString()));
//             ui->tableCustCompany->setItem(row, 2, new QTableWidgetItem(query.value("company_number").toString()));

//             // 팁: UUID를 숨겨두고 싶다면 'id'를 데이터 역할(UserRole)로 저장해둘 수 있습니다.
//             ui->tableCustCompany->item(row, 0)->setData(Qt::UserRole, query.value("id").toString());
//             row++;
//         }
//     }


//}
TestWidget::~TestWidget()
{
    delete ui;
}
