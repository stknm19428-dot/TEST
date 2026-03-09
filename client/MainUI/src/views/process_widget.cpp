#include "process_widget.h"
#include "ui_process_widget.h"
#include "../services/opcua_service.h"
#include "../services/manufacture_service.h"
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QDebug>

ProcessWidget::ProcessWidget(QWidget *parent)
    : BasePageWidget(parent)
    , ui(new Ui::ProcessWidget)
{
    ui->setupUi(this);
    ui->title_label->setText("Process Monitor");
    setup_process_tree();
}

ProcessWidget::~ProcessWidget()
{
    delete ui;
}

void ProcessWidget::setOpcUaService(OpcUaService *uaService) {
    m_ua = uaService;
}

void ProcessWidget::setup_process_tree()
{
    ui->process_tree_widget->setColumnCount(2);
    ui->process_tree_widget->setHeaderLabels({"공정명", "제어"});
    ui->process_tree_widget->setColumnWidth(0, 600);
    ui->process_tree_widget->setColumnWidth(1, 200);

    QTreeWidgetItem *logistics = new QTreeWidgetItem(ui->process_tree_widget);
    logistics->setText(0, "[물류]");
    add_process_item(logistics, "컨베이어 벨트 1");
    add_process_item(logistics, "컨베이어 벨트 2");
    add_process_item(logistics, "컨베이어 벨트 3");

    QTreeWidgetItem *manufacturing = new QTreeWidgetItem(ui->process_tree_widget);
    manufacturing->setText(0, "[제조]");
    add_process_item(manufacturing, "제조 컨테이너 1");

    ui->process_tree_widget->expandAll();
}

void ProcessWidget::add_process_item(QTreeWidgetItem *parent_item, const QString &process_name)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(parent_item);
    item->setText(0, process_name);

    QWidget *container = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    QPushButton *start_btn = new QPushButton("시작");
    start_btn->setFixedWidth(80);
    connect(start_btn, &QPushButton::clicked, this, [this, process_name]() {
        on_start_clicked(process_name);
    });

    QPushButton *stop_btn = new QPushButton("정지");
    stop_btn->setFixedWidth(80);
    connect(stop_btn, &QPushButton::clicked, this, [this, process_name]() {
        on_stop_clicked(process_name);
    });

    layout->addWidget(start_btn);
    layout->addWidget(stop_btn);
    layout->addStretch();

    ui->process_tree_widget->setItemWidget(item, 1, container);
}

void ProcessWidget::on_start_clicked(const QString &process_name)
{
    qDebug() << "[시작 신호]" << process_name;

    if (process_name != "제조 컨테이너 1") {
        qDebug() << "[INFO] belt/manual control is not wired in this flow yet:" << process_name;
        return;
    }

    if (!m_ua) {
        QMessageBox::warning(this, "오류", "OPC UA 서비스가 연결되지 않았습니다.");
        return;
    }

    bool ok = false;
    QString orderId = QInputDialog::getText(this,
                                            "생산 시작",
                                            "Production Order ID 입력:",
                                            QLineEdit::Normal,
                                            "",
                                            &ok).trimmed();

    if (!ok || orderId.isEmpty())
        return;

    const ProductionOrderTask task = ManufactureService::getProductionOrderById(orderId);
    if (!task.valid) {
        QMessageBox::warning(this, "오류", "해당 생산오더를 찾을 수 없습니다.");
        return;
    }

    if (task.status.compare("PENDING", Qt::CaseInsensitive) != 0) {
        QMessageBox::warning(this, "오류", "현재 상태가 PENDING인 오더만 시작할 수 있습니다.");
        return;
    }

    const double speed = task.motorSpeed > 0 ? task.motorSpeed : 100.0;
    m_ua->mfgWriteSpeed(speed);
    m_ua->mfgStartOrder(task.orderId, static_cast<quint16>(task.productNo), static_cast<quint32>(task.orderCount));

    if (!ManufactureService::markProductionOrderInProc(task.orderId)) {
        QMessageBox::warning(this, "오류", "생산오더 상태를 INPROC로 바꾸지 못했습니다.");
        return;
    }

    ManufactureService::createProductLog(task);
    emit productionOrderStarted(task.orderId, task.productId);
}

void ProcessWidget::on_stop_clicked(const QString &process_name)
{
    qDebug() << "[정지 신호]" << process_name;

    if (!m_ua)
        return;

    if (process_name == "제조 컨테이너 1")
        m_ua->mfgStopOrder();
}

void ProcessWidget::on_Back_btn_clicked()
{
    emit requestPageChange(PageType::Dashboard);
}
