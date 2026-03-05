#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "base/base_page_widget.h"
#include "login_widget.h"
#include "dashboard_widget.h"
#include "partner_manage_widget.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    setupNavigation();
    moveToPage(PageType::Login);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::setupNavigation() {
    // 위젯 캐스팅
    auto* login = qobject_cast<BasePageWidget*>(ui->loginPage);
    auto* dashboard = qobject_cast<BasePageWidget*>(ui->dashBoardPage);
    auto* partnerManage = qobject_cast<BasePageWidget*>(ui->partnerManagePage);

    // 모든 위젯을 리스트에 담아 한 번에 연결 (중복 코드 방지)
    QList<BasePageWidget*> pages = {login, dashboard, partnerManage};

    for (BasePageWidget* page : pages) {
        if (page) {
            // Signal: requestPageChange(PageType)
            // Slot: moveToPage(PageType) -> 타입이 일치해야 합니다!
            connect(page, &BasePageWidget::requestPageChange, this, &MainWindow::moveToPage);
        }
    }
}

void MainWindow::moveToPage(PageType type) {
    switch (type) {
    case PageType::Login:
        ui->stackedWidget->setCurrentWidget(ui->loginPage);
        break;
    case PageType::Dashboard:
        ui->stackedWidget->setCurrentWidget(ui->dashBoardPage);
        break;
    case PageType::PartnerManage:
        ui->stackedWidget->setCurrentWidget(ui->partnerManagePage);
        break;
    case PageType::ScmManage:
        ui->stackedWidget->setCurrentWidget(ui->ScmManagePage);
        break;
    }
}
