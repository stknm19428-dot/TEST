#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "base/base_page_widget.h"
#include "login_widget.h"
#include "dashboard_widget.h"
#include "partner_manage_widget.h"
#include "process_widget.h"
#include "../core/database_manager.h"
#include "../services/manufacture_service.h"
#include "../services/scm_manage_service.h"
#include <QDebug>
#include <QFile>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    ua = new OpcUaService(this);

    auto* process = qobject_cast<ProcessWidget*>(ui->processPage);
    if (process) {
        process->setOpcUaService(ua);
        connect(process, &ProcessWidget::productionOrderStarted, this,
                [this](const QString &orderId, const QString &productId) {
                    m_activeProdOrderId = orderId;
                    m_activeProductId = productId;
                    m_lastProdCount = 0;
                    m_lastDefectCount = 0;
                    m_lastAttemptCount = 0;
                    m_waitMaterialStopRequested = false;
                });
    }

    // ✅ 추가
    auto* dashboard = qobject_cast<DashboardWidget*>(ui->dashBoardPage);
    if (dashboard) {
        dashboard->set_opcua_service(ua);
    }

    connect(ua, &OpcUaService::mfgAuthRequestReceived, this,
            [this](const QString &id, const QString &pw){
                const bool ok = m_authService.checkServerAccount("MFG", id, pw);
                qDebug() << "[MES] MFG auth request:" << "id =" << id << "result =" << ok;
                ua->mfgSendAuthResult(ok);
            });

    connect(ua, &OpcUaService::logAuthRequestReceived, this,
            [this](const QString &id, const QString &pw){
                const bool ok = m_authService.checkServerAccount("LOG", id, pw);
                qDebug() << "[MES] LOG auth request:" << "id =" << id << "result =" << ok;
                ua->logSendAuthResult(ok);
            });

    connect(ua, &OpcUaService::logConnectedChanged, this, [this](bool connected) {
        if (!connected)
            return;

        if (!DatabaseManager::instance().connect()) {
            qDebug() << "[MES] DB connect failed -> skip LOG init sync";
            return;
        }

        const WarehouseStockSnapshot snap = ScmManageService::getWarehouseStockSnapshot();
        ua->logInitStocks(snap.wh1, snap.wh2, snap.wh3);
    });

    connect(ua, &OpcUaService::mfgProdCountUpdated, this, [this](quint64 v) {
        if (m_activeProdOrderId.isEmpty() || m_activeProductId.isEmpty())
            return;

        const int prodCount = static_cast<int>(v);
        const int delta = prodCount - m_lastProdCount;
        if (delta <= 0)
            return;

        m_lastProdCount = prodCount;
        ManufactureService::increaseProductStock(m_activeProductId, delta);
        ManufactureService::updateProductLogProgress(m_activeProdOrderId, m_lastProdCount, m_lastDefectCount,
                                                     m_waitMaterialStopRequested ? "WAIT_MAT" : "INPROC");
    });

    connect(ua, &OpcUaService::mfgDefectCountUpdated, this, [this](quint64 v) {
        if (m_activeProdOrderId.isEmpty())
            return;

        m_lastDefectCount = static_cast<int>(v);
        ManufactureService::updateProductLogProgress(m_activeProdOrderId, m_lastProdCount, m_lastDefectCount,
                                                     m_waitMaterialStopRequested ? "WAIT_MAT" : "INPROC");
    });

    connect(ua, &OpcUaService::mfgAttemptCountUpdated, this, [this](quint64 v) {
        if (m_activeProdOrderId.isEmpty() || m_activeProductId.isEmpty())
            return;

        const int attemptCount = static_cast<int>(v);
        const int delta = attemptCount - m_lastAttemptCount;
        if (delta <= 0)
            return;

        m_lastAttemptCount = attemptCount;

        const auto recipe = ManufactureService::getRecipeItemsByProductId(m_activeProductId);
        for (const auto &item : recipe) {
            if (item.warehouseNo < 1 || item.warehouseNo > 3)
                continue;

            const quint32 consumeQty = static_cast<quint32>(item.quantityRequired * delta);
            ua->logConsume(item.warehouseNo, consumeQty);
        }

        ManufactureService::consumeRecipeItems(m_activeProductId, delta);
        ManufactureService::updateProductLogProgress(m_activeProdOrderId, m_lastProdCount, m_lastDefectCount,
                                                     m_waitMaterialStopRequested ? "WAIT_MAT" : "INPROC");
    });

    connect(ua, &OpcUaService::logWhLowStockUpdated, this, [this](int, bool low) {
        if (!low || m_activeProdOrderId.isEmpty() || m_waitMaterialStopRequested)
            return;

        m_waitMaterialStopRequested = true;
        ua->mfgStopOrder();
        ManufactureService::markProductionOrderWaitMaterial(m_activeProdOrderId);
        ManufactureService::updateProductLogProgress(m_activeProdOrderId, m_lastProdCount, m_lastDefectCount, "WAIT_MAT");
    });

    connect(ua, &OpcUaService::mfgStatusUpdated, this, [this](const QString &status) {
        if (m_activeProdOrderId.isEmpty())
            return;

        if (status.compare("Done", Qt::CaseInsensitive) == 0) {
            ManufactureService::markProductionOrderDone(m_activeProdOrderId);
            ManufactureService::updateProductLogProgress(m_activeProdOrderId, m_lastProdCount, m_lastDefectCount, "DONE");
            m_activeProdOrderId.clear();
            m_activeProductId.clear();
            m_lastProdCount = 0;
            m_lastDefectCount = 0;
            m_lastAttemptCount = 0;
            m_waitMaterialStopRequested = false;
        }
    });

    setupNavigation();
    moveToPage(PageType::Login);

    auto* login = qobject_cast<LoginWidget*>(ui->loginPage);
    if (login) {
        connect(login, &LoginWidget::loginSuccess, this, [this](){
            startOpcUaOnce();
        });

        connect(ua, &OpcUaService::mfgSpeedUpdated, this, [](double speed){
            qDebug() << "MFG SPEED =" << speed;
        });

        connect(ua, &OpcUaService::mfgProdCountUpdated, this, [](quint64 v){
            qDebug() << "MFG PRODCOUNT =" << v;
        });
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
    auto* login = qobject_cast<BasePageWidget*>(ui->loginPage);
    auto* dashboard = qobject_cast<BasePageWidget*>(ui->dashBoardPage);
    auto* partnerManage = qobject_cast<BasePageWidget*>(ui->partnerManagePage);
    auto* scmManage = qobject_cast<BasePageWidget*>(ui->ScmManagePage);
    auto* delivery = qobject_cast<BasePageWidget*>(ui->deliveryPage);
    auto* process = qobject_cast<BasePageWidget*>(ui->processPage);
    auto* manufacture = qobject_cast<BasePageWidget*>(ui->manufacturePage);

    QList<BasePageWidget*> pages = {login, dashboard, partnerManage, scmManage, delivery, process, manufacture};

    for (BasePageWidget* page : pages) {
        if (page) {
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
    case PageType::EnvironmentLogs:
        ui->stackedWidget->setCurrentWidget(ui->environmentLogsPage);
        break;
    }
}
