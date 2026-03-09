#pragma once

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QString>
#include <QMutex>

extern "C" {
#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/client_subscriptions.h>
}

class OpcUaService : public QObject {
    Q_OBJECT
public:
    explicit OpcUaService(QObject *parent = nullptr);
    ~OpcUaService();

    // thread loop
    void start();
    void stop();

    // connections (endpoint: opc.tcp://ip:port)
    Q_INVOKABLE void connectMfg(const QString &endpoint,
                                const QString &user,
                                const QString &pass,
                                const QString &clientCertDer,
                                const QString &clientKeyDer,
                                const QString &trustServerDer);

    Q_INVOKABLE void connectLog(const QString &endpoint,
                                const QString &user,
                                const QString &pass,
                                const QString &clientCertDer,
                                const QString &clientKeyDer,
                                const QString &trustServerDer);

    Q_INVOKABLE void disconnectAll();

    // ---- MFG commands ----
    Q_INVOKABLE void mfgWriteSpeed(double speedPercent);    // mfg/conveyor_speed
    Q_INVOKABLE void mfgStartOrder(const QString &orderId, quint16 productNo, quint32 qty);
    Q_INVOKABLE void mfgStopOrder();                        // mfg/StopOrder

    // ---- LOG commands ----
    Q_INVOKABLE void logWriteSpeed(int idx1to3, double speedPercent); // log/conveyor_speed1..3
    Q_INVOKABLE void logMove(int wh1to3, quint32 qty);                // log/Move(wh:uint16, qty:uint32)
    Q_INVOKABLE void logStopMove();                                   // log/StopMove()
    Q_INVOKABLE void logConsume(int wh1to3, quint32 qty);             // log/Consume(wh:uint16, qty:uint32)
    Q_INVOKABLE void logInitStocks(quint32 wh1Qty, quint32 wh2Qty, quint32 wh3Qty);
    Q_INVOKABLE void logWriteArrivalResult(bool ok, const QString &msg);
    Q_INVOKABLE void logClearArrivalRequest();
    // ✅ 서버 로그인 결과 회신
    void mfgSendAuthResult(bool ok);
    void logSendAuthResult(bool ok);

signals:
    void mfgConnectedChanged(bool connected);
    void logConnectedChanged(bool connected);

    // MFG subscriptions
    void mfgStatusUpdated(const QString &status);
    void mfgTempUpdated(double temp);
    void mfgHumUpdated(double hum);
    void mfgSpeedUpdated(double speed);
    void mfgProdCountUpdated(quint64 v);
    void mfgDefectCountUpdated(quint64 v);
    void mfgAttemptCountUpdated(quint64 v);
    void mfgDefectRateUpdated(double v);
    void mfgDefectCodeUpdated(quint16 v);

    // LOG subscriptions
    void logStatusUpdated(const QString &status);
    void logTempUpdated(double temp);
    void logHumUpdated(double hum);

    void logSpeedUpdated(int idx1to3, double speed);

    void logWhLoadingUpdated(int wh1to3, bool loading);
    void logWhLoadedUpdated(int wh1to3, bool loaded);
    void logWhQtyUpdated(int wh1to3, quint32 qty);
    void logWhLowStockUpdated(int wh1to3, bool low);

    void logArrivalRequested(const QString &orderId);

    // ✅ 서버가 보낸 로그인 요청
    void mfgAuthRequestReceived(const QString &id, const QString &pw);
    void logAuthRequestReceived(const QString &id, const QString &pw);

    void errorOccurred(const QString &where, const QString &msg);
    void info(const QString &msg);

private:
    class Worker;
    Worker *m_worker = nullptr;
    QThread m_thread;
};
