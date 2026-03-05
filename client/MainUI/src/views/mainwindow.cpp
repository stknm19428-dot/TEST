#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "login_widget.h"
#include "dashboard_widget.h" // 추가!

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    // 프로그램 시작 시 무조건 로그인 페이지를 보여주도록 명시
    ui->stackedWidget->setCurrentWidget(ui->loginPage);

    LoginWidget* login = qobject_cast<LoginWidget*>(ui->loginPage);
    DashboardWidget* dashboard = qobject_cast<DashboardWidget*>(ui->dashBoardPage); // 캐스팅

    // 웹으로 비유하면 API Gateway (Router)
    // login_widget.ui에서 emit loginSucess시 아래 코드 실행 (Navigation)
    connect(login, &LoginWidget::loginSuccess, this, [this](){
        ui->stackedWidget->setCurrentWidget(ui->dashBoardPage);
    });

    // 대시보드 위젯(dashBoardPage)에서 오는 시그널을 stackedWidget의 setCurrentIndex와 연결
    connect(ui->dashBoardPage, &DashboardWidget::PageChangeCompLists, this, [this](){
        ui->stackedWidget->setCurrentWidget(ui->testPage); // 인덱스 번호 대신 위젯 객체로 직접 지정
    });
}
// 에러 원인 1: 소멸자 구현 누락 해결
MainWindow::~MainWindow()
{
    delete ui;
}
