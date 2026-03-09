#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "base/page_types.h"
#include "opcua_service.h"
#include "auth_service.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    OpcUaService* opcUaService() const { return ua; }

private:
    Ui::MainWindow *ui;

    OpcUaService* ua = nullptr;
    AuthService m_authService;
    bool uaStarted = false;

    QString m_activeProdOrderId;
    QString m_activeProductId;
    int m_lastProdCount = 0;
    int m_lastDefectCount = 0;
    int m_lastAttemptCount = 0;
    bool m_waitMaterialStopRequested = false;

    void startOpcUaOnce();
    void setupNavigation();
    void moveToPage(PageType type);
};

#endif
