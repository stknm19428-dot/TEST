#include "database_manager.h"

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager inst;
    return inst;
}

bool DatabaseManager::connect() {
    if (QSqlDatabase::contains("qt_sql_default_connection")) {
        return true;
    }

    m_db = QSqlDatabase::addDatabase("QMYSQL");
    m_db.setHostName("localhost");
    m_db.setDatabaseName("Smart_MES_Core");
    m_db.setUserName("admin");
    m_db.setPassword("pw1234");

    if (!m_db.open()) {
        qDebug() << "DB Error:" << m_db.lastError().text();
        return false;
    }
    return true;
}
