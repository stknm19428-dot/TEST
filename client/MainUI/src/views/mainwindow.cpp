#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "base/base_page_widget.h"
#include "login_widget.h"
#include "dashboard_widget.h"
#include "partner_manage_widget.h"
#include <QDebug>
#include <QFile>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    ua = new OpcUaService(this);

    // =====================================================
    // ✅ 서버(MFG)가 보낸 로그인 요청 처리
    // =====================================================
    connect(ua, &OpcUaService::mfgAuthRequestReceived, this,
            [this](const QString &id, const QString &pw){
                bool ok = m_authService.checkServerAccount("MFG", id, pw);

                qDebug() << "[MES] MFG auth request:"
                         << "id =" << id
                         << "result =" << ok;

                ua->mfgSendAuthResult(ok);
            });

    // =====================================================
    // ✅ 서버(LOG)가 보낸 로그인 요청 처리
    // =====================================================
    connect(ua, &OpcUaService::logAuthRequestReceived, this,
            [this](const QString &id, const QString &pw){
                bool ok = m_authService.checkServerAccount("LOG", id, pw);

                qDebug() << "[MES] LOG auth request:"
                         << "id =" << id
                         << "result =" << ok;

                ua->logSendAuthResult(ok);
            });
    // =====================================================
    // ✅ 서버(LOG)가 보낸 로그인 요청 처리
    // =====================================================




    setupNavigation();
    moveToPage(PageType::Login);
    auto* login = qobject_cast<LoginWidget*>(ui->loginPage);
    if (login) {
        // ✅ 로그인 성공 “딱 한번”만 통신 시작
        connect(login, &LoginWidget::loginSuccess, this, [this](){
            startOpcUaOnce();
        });
        // connect(ua, &OpcUaService::mfgTempUpdated, this, [](double temp){
        //     qDebug() << "MFG TEMP =" << temp;
        // });
        // connect(ua, &OpcUaService::mfgHumUpdated, this, [](double hum){
        //     qDebug() << "MFG HUM =" << hum;
        // });

        connect(ua, &OpcUaService::mfgSpeedUpdated, this, [](double speed){
            qDebug() << "MFG SPEED =" << speed;
        });

        connect(ua, &OpcUaService::mfgProdCountUpdated, this, [](quint64 v){
            qDebug() << "MFG PRODCOUNT =" << v;
        });


        // connect(ua, &OpcUaService::logTempUpdated, this, [](double t){
        //     qDebug() << "LOG TEMP =" << t;
        // });

    }


}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::startOpcUaOnce()
{
    qDebug() << QFile::exists("/home/pi/MES/servers/certs/mfg/cert.der");
    if (uaStarted) return;
    uaStarted = true;

    ua->connectMfg(
        "opc.tcp://10.10.16.208:4850",
        "mes","mespw_change_me",
        "/home/pi/MES/servers/certs/mes/cert.der",
        "/home/pi/MES/servers/certs/mes/key.der",
        "/home/pi/MES/servers/certs/mes/trust_mfg.der"
        );

    ua->connectLog(
        "opc.tcp://10.10.16.210:4841",
        "mes","mespw_change_me",
        "/home/pi/MES/servers/certs/mes/cert.der",
        "/home/pi/MES/servers/certs/mes/key.der",
        "/home/pi/MES/servers/certs/mes/trust_log.der"
        );


}


void MainWindow::setupNavigation() {
    // 위젯 캐스팅
    auto* login = qobject_cast<BasePageWidget*>(ui->loginPage);
    auto* dashboard = qobject_cast<BasePageWidget*>(ui->dashBoardPage);
    auto* partnerManage = qobject_cast<BasePageWidget*>(ui->partnerManagePage);
    auto* scmManage = qobject_cast<BasePageWidget*>(ui->ScmManagePage);
    auto* delivery = qobject_cast<BasePageWidget*>(ui->deliveryPage);
    auto* process = qobject_cast<BasePageWidget*>(ui->processPage);
    auto* manufacture = qobject_cast<BasePageWidget*>(ui->manufacturePage);

    // 모든 위젯을 리스트에 담아 한 번에 연결 (중복 코드 방지)
    QList<BasePageWidget*> pages = {login, dashboard, partnerManage, scmManage, delivery, process, manufacture};

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
    case PageType::Delivery:
        ui->stackedWidget->setCurrentWidget(ui->deliveryPage);
        break;
    case PageType::Process:
        ui->stackedWidget->setCurrentWidget(ui->processPage);
        break;
    case PageType::Manufacture:
        ui->stackedWidget->setCurrentWidget(ui->manufacturePage);
        break;
    }
}
