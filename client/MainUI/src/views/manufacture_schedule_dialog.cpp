#include "manufacture_schedule_dialog.h"
#include "ui_manufacture_schedule_dialog.h"
#include <QSqlDatabase>

ManufactureScheduleDialog::ManufactureScheduleDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ManufactureScheduleDialog)
{
    ui->setupUi(this);
    ui->deadline_edit->setDateTime(QDateTime::currentDateTime());
    load_products();
}

ManufactureScheduleDialog::~ManufactureScheduleDialog()
{
    delete ui;
}

void ManufactureScheduleDialog::load_products()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "DB 연결이 열려있지 않습니다.";
        return;
    }

    QSqlQuery query("SELECT id, product_code, product_name FROM product", db);
    if (!query.exec()) {
        qDebug() << "제품 조회 에러:" << query.lastError().text();
        return;
    }

    product_id_list.clear();
    ui->product_combo->clear();

    while (query.next()) {
        QString id   = query.value("id").toString();
        QString code = query.value("product_code").toString();
        QString name = query.value("product_name").toString();
        ui->product_combo->addItem(QString("[%1] %2").arg(code, name));
        product_id_list.append(id);
    }
}

void ManufactureScheduleDialog::on_confirm_button_clicked()
{
    int index = ui->product_combo->currentIndex();
    if (index < 0) {
        qDebug() << "제품을 선택해주세요.";
        return;
    }

    QString product_id  = product_id_list[index];
    int order_count     = ui->order_count_edit->text().toInt();
    int motor_speed     = ui->motor_speed_edit->text().toInt();
    QString deadline    = ui->deadline_edit->dateTime().toString("yyyy-MM-dd HH:mm:ss");

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "DB 연결이 열려있지 않습니다.";
        return;
    }

    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO product_order_logs (id, product_id, order_count, motor_speed, status, deadline_at) "
        "VALUES (UUID(), :product_id, :order_count, :motor_speed, 'PENDING', :deadline_at)"
        );
    query.bindValue(":product_id",  product_id);
    query.bindValue(":order_count", order_count);
    query.bindValue(":motor_speed", motor_speed);
    query.bindValue(":deadline_at", deadline);

    if (query.exec()) {
        qDebug() << "[스케줄 생성 성공]";
        accept();
    } else {
        qDebug() << "[스케줄 생성 실패]" << query.lastError().text();
    }
}

void ManufactureScheduleDialog::on_cancel_button_clicked()
{
    reject();
}
