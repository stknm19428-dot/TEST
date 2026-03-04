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

    connect(login, &LoginWidget::loginSuccess, this, [this](){
        ui->stackedWidget->setCurrentWidget(ui->dashBoardPage);

        // 대시보드 위젯을 가져와서 차트 초기화
        DashboardWidget* dashboard = qobject_cast<DashboardWidget*>(ui->dashBoardPage);
        if (dashboard) dashboard->initChart();
    });
}
// 에러 원인 1: 소멸자 구현 누락 해결
MainWindow::~MainWindow()
{
    delete ui;
}
