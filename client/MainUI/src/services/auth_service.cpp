#include "auth_service.h"
#include "../core/user_session.h" // 세션 싱글톤 헤더 추가

bool AuthService::authenticate(const QString& username, const QString& password) {
    QSqlQuery query;
    query.prepare("SELECT u.id, u.user_name, p.password_hash, p.salt FROM user u "
                  "JOIN user_password p ON u.id = p.user_id "
                  "WHERE u.user_name = :username");
    query.bindValue(":username", username);

    if (!query.exec() || !query.next()) return false;

    // 값 추출 (0:id, 1:username, 2:hash, 3:salt)
    QString userId = query.value(0).toString();
    QString userName = query.value(1).toString();
    QString db_hash = query.value(2).toString();
    QString db_salt = query.value(3).toString();

    // 비밀번호 검증
    if (db_hash == sha512_hash(password, db_salt)) {
        // ✅ [SUCCESS] 웹의 세션 생성과 동일: 싱글톤에 유저 정보 기록
        UserSession::instance().login(userId, userName);
        return true;
    }

    return false;
}
// ✅ 서버 로그인 검증용

bool AuthService::checkServerAccount(const QString& serverType,
                                     const QString& username,
                                     const QString& password) {
    Q_UNUSED(serverType);

    QSqlQuery query;
    query.prepare("SELECT u.id, u.user_name, u.role, p.password_hash, p.salt "
                  "FROM user u "
                  "JOIN user_password p ON u.id = p.user_id "
                  "WHERE u.user_name = :username");
    query.bindValue(":username", username);

    if (!query.exec() || !query.next())
        return false;

    QString role    = query.value(2).toString();
    QString db_hash = query.value(3).toString();
    QString db_salt = query.value(4).toString();

    if (db_hash != sha512_hash(password, db_salt))
        return false;

    return (role == "SYS_ADMIN" || role == "admin");
}

QString AuthService::sha512_hash(const QString &password, const QString &salt) {
    QByteArray salted = (password + salt).toUtf8();
    return QCryptographicHash::hash(salted, QCryptographicHash::Sha512).toHex();
}
