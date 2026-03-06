#ifndef AUTH_SERVICE_H
#define AUTH_SERVICE_H

#include <QString>
#include <QSqlQuery>
#include <QCryptographicHash>

class AuthService {
public:
    bool authenticate(const QString& username, const QString& password);

    // ✅ 서버 로그인 확인용 추가
    bool checkServerAccount(const QString& serverType,
                            const QString& username,
                            const QString& password);

private:
    QString sha512_hash(const QString& password, const QString& salt);
};

#endif
