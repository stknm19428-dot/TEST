#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "base/page_types.h" // 1. 이 헤더가 반드시 포함되어야 합니다!
#include "opcua_service.h"
//========================
//
//=========================
#include "auth_service.h"   // ✅ 추가
//========================
//
//=========================


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    OpcUaService* ua = nullptr;
    //========================
    //
    //=========================
    AuthService m_authService;   // ✅ 추가
    //========================
    //
    //=========================
    bool uaStarted = false;
    void startOpcUaOnce();

    void setupNavigation();
    // 2. 에러 메시지에서 'int'로 인식되던 부분을 'PageType'으로 명확히 수정합니다.
    void moveToPage(PageType type);
};

#endif
