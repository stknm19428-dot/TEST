#include "order_edit_dialog.h"
#include "ui_order_edit_dialog.h"

OrderEditDialog::OrderEditDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::OrderEditDialog)
{
    ui->setupUi(this);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &OrderEditDialog::updateDateTime);
    timer->start(1000); // 1000ms = 1s

    updateDateTime();
}

void OrderEditDialog::loadInventoryList() {
    QSqlQuery query("SELECT item_code, item_name FROM inventory");

    ui->comboItemName->clear(); // 초기화

    while (query.next()) {
        QString itemCode = query.value("item_code").toString();
        QString itemName = query.value("item_name").toString();

        // 화면에 보여줄 텍스트: [코드] 이름
        QString displayText = QString("[%1] %2").arg(itemCode, itemName);

        // 💡 중요:addItem(보여줄 글자, 실제 데이터)
        // 나중에 INSERT 쿼리에서 쓸 itemCode를 데이터로 함께 저장
        ui->comboItemName->addItem(displayText, itemCode);
    }
}

QString OrderEditDialog::getSelectedItemCode() const {
    return ui->comboItemName->currentData().toString();
}

int OrderEditDialog::getOrderAmount() const {
    return ui->spinAmount->value();
}

void OrderEditDialog::updateDateTime(){
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    ui->labelCurrentTime->setText(currentTime);
}

OrderEditDialog::~OrderEditDialog()
{
    delete ui;
}
