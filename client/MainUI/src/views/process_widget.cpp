#include "process_widget.h"
#include "ui_process_widget.h"
#include <QPushButton>
#include <QDebug>

ProcessWidget::ProcessWidget(QWidget *parent)
    : QWidget(parent)
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

void ProcessWidget::setup_process_tree()
{
    ui->process_tree_widget->setColumnCount(2);
    ui->process_tree_widget->setHeaderLabels({"공정명", "제어"});
    ui->process_tree_widget->setColumnWidth(0, 600);
    ui->process_tree_widget->setColumnWidth(1, 200);

    // ── 물류 카테고리 ──
    QTreeWidgetItem *logistics = new QTreeWidgetItem(ui->process_tree_widget);
    logistics->setText(0, "[물류]");

    add_process_item(logistics, "컨베이어 벨트 1");
    add_process_item(logistics, "컨베이어 벨트 2");
    add_process_item(logistics, "컨베이어 벨트 3");

    // ── 제조 카테고리 ──
    QTreeWidgetItem *manufacturing = new QTreeWidgetItem(ui->process_tree_widget);
    manufacturing->setText(0, "[제조]");

    add_process_item(manufacturing, "제조 컨테이너 1");

    ui->process_tree_widget->expandAll();
}

void ProcessWidget::add_process_item(QTreeWidgetItem *parent_item,
                                     const QString &process_name)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(parent_item);
    item->setText(0, process_name);

    QPushButton *stop_btn = new QPushButton("정지");
    stop_btn->setFixedWidth(80);

    connect(stop_btn, &QPushButton::clicked, this, [this, process_name]() {
        on_stop_clicked(process_name);
    });

    ui->process_tree_widget->setItemWidget(item, 1, stop_btn);
}

void ProcessWidget::on_stop_clicked(const QString &process_name)
{
    qDebug() << "[정지 신호]" << process_name;
}
