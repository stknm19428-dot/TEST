#include "auth_service.h"

bool AuthService::authenticate(const QString& username, const QString& password) {
    QSqlQuery query;
    query.prepare("SELECT p.password_hash, p.salt FROM user u "
                  "JOIN user_password p ON u.id = p.user_id "
                  "WHERE u.user_name = :username");
    query.bindValue(":username", username);

    if (!query.exec() || !query.next()) return false;

    QString db_hash = query.value(0).toString();
    QString db_salt = query.value(1).toString();

    return (db_hash == sha512_hash(password, db_salt));
}

QString AuthService::sha512_hash(const QString &password, const QString &salt) {
    QByteArray salted = (password + salt).toUtf8();
    return QCryptographicHash::hash(salted, QCryptographicHash::Sha512).toHex();
}
