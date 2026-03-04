#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <QSqlDatabase>
#include <QSqlError>
#include <QDebug>

class DatabaseManager {
public:
    static DatabaseManager& instance();
    bool connect();
    void close();

private:
    DatabaseManager() = default;
    ~DatabaseManager() = default;
    QSqlDatabase m_db;
};

#endif
