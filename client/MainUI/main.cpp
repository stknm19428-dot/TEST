#include "mainwindow.h"
#include "database_manager.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 시작 시 DB 연결
    if (!DatabaseManager::instance().connect()) {
        return -1;
    }

    MainWindow w;
    w.show();
    return a.exec();
}
