#include "delivery_dialog.h"
#include "ui_delivery_dialog.h"

DeliveryDialog::DeliveryDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DeliveryDialog)
{
    ui->setupUi(this);
    ui->title_label->setText("Delivery Order");

    // product, customer 데이터 불러오기
    load_products();
    load_customers();
}

DeliveryDialog::~DeliveryDialog()
{
    delete ui;
}

void DeliveryDialog::load_products()
{
    auto products = DeliveryService::getProducts();

    product_id_list.clear();
    product_code_list.clear();
    ui->product_combo->clear();

    for (const ProductInfo &p : products) {
        product_id_list.append(p.id);
        product_code_list.append(p.code);
        ui->product_combo->addItem(p.code); // ComboBox에 code 표시
    }
    qDebug() << "총" << product_id_list.size() << "개의 제품을 불러왔습니다.";
}

void DeliveryDialog::load_customers()
{
    auto customers = DeliveryService::getCustomers();

    company_id_list.clear();
    company_name_list.clear();
    ui->company_combo->clear();

    for (const CompanyInfo &c : customers) {
        company_id_list.append(c.id);
        company_name_list.append(c.name);
        ui->company_combo->addItem(c.name); // ComboBox에 name 표시
    }
    qDebug() << "총" << company_id_list.size() << "개의 고객사를 불러왔습니다.";
}

void DeliveryDialog::on_confirm_button_clicked()
{
    // 입력값 수집
    int product_index = ui->product_combo->currentIndex();
    int company_index = ui->company_combo->currentIndex();
    QString stock     = ui->stock_edit->text();

    // 입력값 검증
    if (product_index < 0 || company_index < 0 || stock.isEmpty()) {
        qDebug() << "입력값이 비어있습니다.";
        return;
    }

    // 배열에서 id 가져오기
    QString product_id = product_id_list[product_index];
    QString company_id = company_id_list[company_index];

    // DB 연결 확인
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "DB 연결이 열려있지 않습니다.";
        return;
    }

    // SQL INSERT 실행
    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO product_deli_logs "
        "(id, company_id, product_id, delivery_stock, status) "
        "VALUES (UUID(), :company_id, :product_id, :delivery_stock, 'PENDING')"
        );
    query.bindValue(":company_id",      company_id);
    query.bindValue(":product_id",      product_id);
    query.bindValue(":delivery_stock",  stock.toInt());

    if (query.exec()) {
        qDebug() << "[납품 지시 생성 성공]";
        accept(); // 팝업 닫기
    } else {
        qDebug() << "[납품 지시 생성 실패]" << query.lastError().text();
    }
}

void DeliveryDialog::on_cancel_button_clicked()
{
    reject(); // 팝업 닫기
}
