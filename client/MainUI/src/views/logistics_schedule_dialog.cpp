#include "logistics_schedule_dialog.h"
#include "ui_logistics_schedule_dialog.h"
#include <QDebug>

LogisticsScheduleDialog::LogisticsScheduleDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LogisticsScheduleDialog)
{
    ui->setupUi(this);
    ui->title_label->setText("Logistics Schedule");
}

LogisticsScheduleDialog::~LogisticsScheduleDialog()
{
    delete ui;
}

void LogisticsScheduleDialog::on_confirm_button_clicked()
{
    QString item_name = ui->item_name_edit->text();
    QString quantity  = ui->quantity_edit->text();
    QString supplier  = ui->supplier_edit->text();
    QString date      = ui->inbound_date_edit->date().toString("yyyy-MM-dd");

    qDebug() << "[입고 스케줄 생성]";
    qDebug() << "Item:" << item_name;
    qDebug() << "Quantity:" << quantity;
    qDebug() << "Supplier:" << supplier;
    qDebug() << "Date:" << date;

    // TODO: DB 저장 로직 연결
    accept(); // 다이얼로그 닫기
}

void LogisticsScheduleDialog::on_cancel_button_clicked()
{
    reject(); // 다이얼로그 닫기
}
