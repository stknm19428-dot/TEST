#include "process_widget.h"
#include "ui_process_widget.h"
#include <QPushButton>
#include <QDebug>

ProcessWidget::ProcessWidget(QWidget *parent)
    : BasePageWidget(parent)  // 0308 haesung changed it 'QWidget' into 'BasePageWidget'
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

    // 1. 버튼들을 담을 컨테이너 위젯과 수평 레이아웃 생성
    QWidget *container = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(container);

    // 레이아웃 간격 및 여백 조정 (표 안에서 예쁘게 보이도록)
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    // 2. 시작 버튼 생성 및 연결
    QPushButton *start_btn = new QPushButton("시작");
    start_btn->setFixedWidth(80);
    connect(start_btn, &QPushButton::clicked, this, [this, process_name]() {
        qDebug() << "[시작 신호]" << process_name;
    });

    // 3. 정지 버튼 생성 및 연결
    QPushButton *stop_btn = new QPushButton("정지");
    stop_btn->setFixedWidth(80);
    connect(stop_btn, &QPushButton::clicked, this, [this, process_name]() {
        on_stop_clicked(process_name);
    });

    // 4. 레이아웃에 버튼 추가
    layout->addWidget(start_btn);
    layout->addWidget(stop_btn);
    layout->addStretch(); // 버튼들을 왼쪽으로 정렬

    ui->process_tree_widget->setItemWidget(item, 1, container);
}


void ProcessWidget::on_start_clicked(const QString &process_name)
{
    qDebug() << "[시작 신호]" << process_name;
    // TODO: 여기에 OPC UA 서버로 시작 명령을 보내는 로직을 추가하면 됩니다.
}
void ProcessWidget::on_stop_clicked(const QString &process_name)
{
    qDebug() << "[정지 신호]" << process_name;
}

void ProcessWidget::on_Back_btn_clicked()
{
    emit requestPageChange(PageType::Dashboard); // Haesung : why does it make 'requestPageChange was not declared in this scope' error
                                                 //           especially on this widget??
                                                 // fixed it by [0308 haesung changed it 'QWidget' into 'BasePageWidget'] !!
}

