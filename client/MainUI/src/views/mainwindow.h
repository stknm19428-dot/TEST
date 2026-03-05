#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "base/page_types.h" // 1. 이 헤더가 반드시 포함되어야 합니다!

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

    void setupNavigation();
    // 2. 에러 메시지에서 'int'로 인식되던 부분을 'PageType'으로 명확히 수정합니다.
    void moveToPage(PageType type);
};

#endif
