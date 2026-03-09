#include "opcua_service.h"

#include <QMetaObject>
#include <QFile>
#include <QDebug>
#include <QTimer>
#include <QMutex>
#include <QDateTime>
#include <QMutexLocker>
#include <utility>

static UA_ByteString loadFileBytes(const QString &path) {
    UA_ByteString bs = UA_BYTESTRING_NULL;
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly)) return bs;

    QByteArray data = f.readAll();
    bs.length = (size_t)data.size();
    bs.data = (UA_Byte*)UA_malloc(bs.length);
    if(!bs.data) {
        bs.length = 0;
        return bs;
    }

    memcpy(bs.data, data.constData(), bs.length);
    return bs;
}

static QString sc(UA_StatusCode rc) {
#ifdef UA_STATUSCODE_NAME
    return QString::fromUtf8(UA_StatusCode_name(rc));
#else
    return QString("0x%1").arg(QString::number(rc, 16));
#endif
}

// ---------- NodeId helpers ----------
static inline UA_NodeId nid(const char *s) { return UA_NODEID_STRING(1, (char*)s); }

// MFG
static const char *MFG_STATUS          = "mfg/status";
static const char *MFG_TEMP            = "mfg/temp";
static const char *MFG_HUM             = "mfg/hum";
static const char *MFG_SPEED           = "mfg/conveyor_speed";
static const char *MFG_PROD_COUNT      = "mfg/prod_count";
static const char *MFG_DEFECT_COUNT    = "mfg/defect_count";
static const char *MFG_DEFECT_RATE     = "mfg/defect_rate";
static const char *MFG_DEFECT_CODE     = "mfg/defect_code";
static const char *MFG_ATTEMPT_COUNT   = "mfg/attempt_count";
static const char *MFG_STARTORDER_M    = "mfg/StartOrder";
static const char *MFG_STOPORDER_M     = "mfg/StopOrder";

// LOG
static const char *LOG_STATUS          = "log/status";
static const char *LOG_TEMP            = "log/temp";
static const char *LOG_HUM             = "log/hum";
static const char *LOG_SPEED1          = "log/conveyor_speed1";
static const char *LOG_SPEED2          = "log/conveyor_speed2";
static const char *LOG_SPEED3          = "log/conveyor_speed3";

static const char *LOG_WH_LOADING[3]   = {"log/wh1/loading","log/wh2/loading","log/wh3/loading"};
static const char *LOG_WH_LOADED[3]    = {"log/wh1/loaded","log/wh2/loaded","log/wh3/loaded"};
static const char *LOG_WH_QTY[3]       = {"log/wh1/load_qty","log/wh2/load_qty","log/wh3/load_qty"};
static const char *LOG_WH_LOW[3]       = {"log/wh1/low_stock","log/wh2/low_stock","log/wh3/low_stock"};

static const char *LOG_MOVE_M          = "log/Move";
static const char *LOG_STOPMOVE_M      = "log/StopMove";
static const char *LOG_INITSTOCKS_M    = "log/InitStocks";
static const char *LOG_CONSUME_M       = "log/Consume";
// MFG AUTH
static const char *MFG_AUTH_REQ_ID       = "mfg/auth/request_id";
static const char *MFG_AUTH_REQ_PW       = "mfg/auth/request_pw";
static const char *MFG_AUTH_REQ_PENDING  = "mfg/auth/request_pending";
static const char *MFG_AUTH_RESULT_OK    = "mfg/auth/result_ok";
static const char *MFG_AUTH_RESULT_DONE  = "mfg/auth/result_done";

// LOG AUTH
static const char *LOG_AUTH_REQ_ID       = "log/auth/request_id";
static const char *LOG_AUTH_REQ_PW       = "log/auth/request_pw";
static const char *LOG_AUTH_REQ_PENDING  = "log/auth/request_pending";
static const char *LOG_AUTH_RESULT_OK    = "log/auth/result_ok";
static const char *LOG_AUTH_RESULT_DONE  = "log/auth/result_done";
static const char *LOG_ARRIVAL_ORDER_ID    = "log/arrival/order_id";
static const char *LOG_ARRIVAL_PENDING     = "log/arrival/pending";
static const char *LOG_ARRIVAL_DONE        = "log/arrival/done";
static const char *LOG_ARRIVAL_OK          = "log/arrival/ok";
static const char *LOG_ARRIVAL_MSG         = "log/arrival/msg";


// tag struct for monitored items
enum class TagKind {
    // string
    MFG_STATUS, LOG_STATUS,
    // doubles
    MFG_TEMP, MFG_HUM, MFG_SPEED, MFG_DEFECT_RATE,
    LOG_TEMP, LOG_HUM,
    LOG_SPEED1, LOG_SPEED2, LOG_SPEED3,
    // u64/u32/u16/bool
    MFG_PROD_COUNT, MFG_DEFECT_COUNT, MFG_ATTEMPT_COUNT, MFG_DEFECT_CODE,
    WH_LOADING_1, WH_LOADING_2, WH_LOADING_3,
    WH_LOADED_1,  WH_LOADED_2,  WH_LOADED_3,
    WH_QTY_1,     WH_QTY_2,     WH_QTY_3,
    WH_LOW_1,     WH_LOW_2,     WH_LOW_3,
    MFG_AUTH_PENDING,
    LOG_AUTH_PENDING,
    LOG_ARRIVAL_PENDING,
};

struct MonTag {
    TagKind kind;
};

// ---------------- Worker ----------------
class OpcUaService::Worker : public QObject {
    Q_OBJECT
public:
    Worker() = default;
    ~Worker() override { cleanup(); }

public slots:
    void startLoop() {
        if(loopTimer) return;

        loopTimer = new QTimer(this);
        loopTimer->setInterval(30);
        connect(loopTimer, &QTimer::timeout, this, &Worker::iterate);
        loopTimer->start();

        emit info("OPC UA loop started");
    }

    void stopLoop() {
        if(loopTimer) {
            loopTimer->stop();
            loopTimer->deleteLater();
            loopTimer = nullptr;
        }

        cleanup();
        emit info("OPC UA loop stopped");
    }

    void connectMfg(QString endpoint, QString user, QString pass,
                    QString clientCertDer, QString clientKeyDer, QString trustServerDer) {
        QMutexLocker lk(&mu);

        mfg.endpoint = endpoint;
        mfg.user = user;
        mfg.pass = pass;
        mfg.clientCertPath = clientCertDer;
        mfg.clientKeyPath = clientKeyDer;
        mfg.trustSrvPath = trustServerDer;
        mfg.reconnectEnabled = true;

        connectOne(mfg, endpoint, user, pass, clientCertDer, clientKeyDer, trustServerDer, true);
    }

    void connectLog(QString endpoint, QString user, QString pass,
                    QString clientCertDer, QString clientKeyDer, QString trustServerDer) {
        QMutexLocker lk(&mu);

        log.endpoint = endpoint;
        log.user = user;
        log.pass = pass;
        log.clientCertPath = clientCertDer;
        log.clientKeyPath = clientKeyDer;
        log.trustSrvPath = trustServerDer;
        log.reconnectEnabled = true;

        connectOne(log, endpoint, user, pass, clientCertDer, clientKeyDer, trustServerDer, false);
    }

    void disconnectAll() {
        QMutexLocker lk(&mu);

        mfg.reconnectEnabled = false;
        log.reconnectEnabled = false;

        disconnectOneGraceful(mfg, true);
        disconnectOneGraceful(log, false);
    }

    // ---- MFG API ----
    void mfgWriteSpeed(double pct) {
        QMutexLocker lk(&mu);
        if(!mfg.client || !mfg.connected) {
            emit errorOccurred("mfgWriteSpeed", "MFG not connected");
            return;
        }

        double v = clampPct(pct);
        UA_Variant var;
        UA_Variant_setScalar(&var, &v, &UA_TYPES[UA_TYPES_DOUBLE]);

        UA_StatusCode rc = UA_Client_writeValueAttribute(mfg.client, nid(MFG_SPEED), &var);
        if(rc != UA_STATUSCODE_GOOD)
            emit errorOccurred("mfgWriteSpeed", "write failed: " + sc(rc));
    }

    void mfgStartOrder(const QString &orderId, quint16 productNo, quint32 qty) {
        QMutexLocker lk(&mu);
        if(!mfg.client || !mfg.connected) {
            emit errorOccurred("mfgStartOrder", "MFG not connected");
            return;
        }

        QByteArray ba = orderId.toUtf8();
        UA_String s;
        s.length = (size_t)ba.size();
        s.data = (UA_Byte*)ba.data();

        UA_UInt16 p = (UA_UInt16)productNo;
        UA_UInt32 q = (UA_UInt32)qty;

        UA_Variant in[3];
        UA_Variant_init(&in[0]);
        UA_Variant_init(&in[1]);
        UA_Variant_init(&in[2]);

        UA_Variant_setScalar(&in[0], &s, &UA_TYPES[UA_TYPES_STRING]);
        UA_Variant_setScalar(&in[1], &p, &UA_TYPES[UA_TYPES_UINT16]);
        UA_Variant_setScalar(&in[2], &q, &UA_TYPES[UA_TYPES_UINT32]);

        UA_StatusCode rc = UA_Client_call(mfg.client,
                                          UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                          nid(MFG_STARTORDER_M),
                                          3, in,
                                          0, nullptr);

        if(rc != UA_STATUSCODE_GOOD)
            emit errorOccurred("mfgStartOrder", "call failed: " + sc(rc));
    }

    void mfgStopOrder() {
        QMutexLocker lk(&mu);
        if(!mfg.client || !mfg.connected) {
            emit errorOccurred("mfgStopOrder", "MFG not connected");
            return;
        }

        UA_StatusCode rc = UA_Client_call(mfg.client,
                                          UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                          nid(MFG_STOPORDER_M),
                                          0, nullptr,
                                          0, nullptr);
        if(rc != UA_STATUSCODE_GOOD)
            emit errorOccurred("mfgStopOrder", "call failed: " + sc(rc));
    }

    // ---- LOG API ----
    void logWriteSpeed(int idx1to3, double pct) {
        QMutexLocker lk(&mu);
        if(!log.client || !log.connected) {
            emit errorOccurred("logWriteSpeed", "LOG not connected");
            return;
        }
        if(idx1to3 < 1 || idx1to3 > 3) {
            emit errorOccurred("logWriteSpeed", "idx out of range");
            return;
        }

        const char *node = (idx1to3==1)?LOG_SPEED1:(idx1to3==2)?LOG_SPEED2:LOG_SPEED3;
        double v = clampPct(pct);

        UA_Variant var;
        UA_Variant_setScalar(&var, &v, &UA_TYPES[UA_TYPES_DOUBLE]);

        UA_StatusCode rc = UA_Client_writeValueAttribute(log.client, nid(node), &var);
        if(rc != UA_STATUSCODE_GOOD)
            emit errorOccurred("logWriteSpeed", "write failed: " + sc(rc));
    }

    void logMove(int wh1to3, quint32 qty) {
        QMutexLocker lk(&mu);
        if(!log.client || !log.connected) {
            emit errorOccurred("logMove", "LOG not connected");
            return;
        }
        if(wh1to3 < 1 || wh1to3 > 3) {
            emit errorOccurred("logMove", "wh out of range");
            return;
        }

        UA_UInt16 wh = (UA_UInt16)wh1to3;
        UA_UInt32 q  = (UA_UInt32)qty;

        UA_Variant in[2];
        UA_Variant_init(&in[0]);
        UA_Variant_init(&in[1]);
        UA_Variant_setScalar(&in[0], &wh, &UA_TYPES[UA_TYPES_UINT16]);
        UA_Variant_setScalar(&in[1], &q,  &UA_TYPES[UA_TYPES_UINT32]);

        UA_StatusCode rc = UA_Client_call(log.client,
                                          UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                          nid(LOG_MOVE_M),
                                          2, in,
                                          0, nullptr);
        if(rc != UA_STATUSCODE_GOOD)
            emit errorOccurred("logMove", "call failed: " + sc(rc));
    }

    void logStopMove() {
        QMutexLocker lk(&mu);
        if(!log.client || !log.connected) {
            emit errorOccurred("logStopMove", "LOG not connected");
            return;
        }

        UA_StatusCode rc = UA_Client_call(log.client,
                                          UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                          nid(LOG_STOPMOVE_M),
                                          0, nullptr,
                                          0, nullptr);
        if(rc != UA_STATUSCODE_GOOD)
            emit errorOccurred("logStopMove", "call failed: " + sc(rc));
    }

    void logConsume(int wh1to3, quint32 qty) {
        QMutexLocker lk(&mu);
        if(!log.client || !log.connected) {
            emit errorOccurred("logConsume", "LOG not connected");
            return;
        }
        if(wh1to3 < 1 || wh1to3 > 3) {
            emit errorOccurred("logConsume", "wh out of range");
            return;
        }

        UA_UInt16 wh = (UA_UInt16)wh1to3;
        UA_UInt32 q  = (UA_UInt32)qty;

        UA_Variant in[2];
        UA_Variant_init(&in[0]);
        UA_Variant_init(&in[1]);
        UA_Variant_setScalar(&in[0], &wh, &UA_TYPES[UA_TYPES_UINT16]);
        UA_Variant_setScalar(&in[1], &q,  &UA_TYPES[UA_TYPES_UINT32]);

        UA_StatusCode rc = UA_Client_call(log.client,
                                          UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                          nid(LOG_CONSUME_M),
                                          2, in,
                                          0, nullptr);
        if(rc != UA_STATUSCODE_GOOD)
            emit errorOccurred("logConsume", "call failed: " + sc(rc));
    }

    void logInitStocks(quint32 wh1Qty, quint32 wh2Qty, quint32 wh3Qty) {
        QMutexLocker lk(&mu);
        if(!log.client || !log.connected) {
            emit errorOccurred("logInitStocks", "LOG not connected");
            return;
        }

        UA_UInt32 q1 = (UA_UInt32)wh1Qty;
        UA_UInt32 q2 = (UA_UInt32)wh2Qty;
        UA_UInt32 q3 = (UA_UInt32)wh3Qty;

        UA_Variant in[3];
        UA_Variant_init(&in[0]);
        UA_Variant_init(&in[1]);
        UA_Variant_init(&in[2]);
        UA_Variant_setScalar(&in[0], &q1, &UA_TYPES[UA_TYPES_UINT32]);
        UA_Variant_setScalar(&in[1], &q2, &UA_TYPES[UA_TYPES_UINT32]);
        UA_Variant_setScalar(&in[2], &q3, &UA_TYPES[UA_TYPES_UINT32]);

        UA_StatusCode rc = UA_Client_call(log.client,
                                          UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                          nid(LOG_INITSTOCKS_M),
                                          3, in,
                                          0, nullptr);
        if(rc != UA_STATUSCODE_GOOD)
            emit errorOccurred("logInitStocks", "call failed: " + sc(rc));
    }
    void logWriteArrivalResult(bool ok, const QString &msg) {
        QMutexLocker lk(&mu);
        if(!log.client || !log.connected) {
            emit errorOccurred("logWriteArrivalResult", "LOG not connected");
            return;
        }

        UA_Boolean bOk = ok ? true : false;
        UA_Boolean bDone = true;
        UA_Boolean bPending = false;

        QByteArray ba = msg.toUtf8();
        UA_String s;
        s.length = (size_t)ba.size();
        s.data = (UA_Byte*)ba.data();

        UA_Variant vOk, vDone, vPending, vMsg;
        UA_Variant_setScalar(&vOk, &bOk, &UA_TYPES[UA_TYPES_BOOLEAN]);
        UA_Variant_setScalar(&vDone, &bDone, &UA_TYPES[UA_TYPES_BOOLEAN]);
        UA_Variant_setScalar(&vPending, &bPending, &UA_TYPES[UA_TYPES_BOOLEAN]);
        UA_Variant_setScalar(&vMsg, &s, &UA_TYPES[UA_TYPES_STRING]);

        UA_Client_writeValueAttribute(log.client, nid(LOG_ARRIVAL_OK), &vOk);
        UA_Client_writeValueAttribute(log.client, nid(LOG_ARRIVAL_DONE), &vDone);
        UA_Client_writeValueAttribute(log.client, nid(LOG_ARRIVAL_PENDING), &vPending);
        UA_Client_writeValueAttribute(log.client, nid(LOG_ARRIVAL_MSG), &vMsg);
    }

    void logClearArrivalRequest() {
        QMutexLocker lk(&mu);
        if(!log.client || !log.connected) {
            emit errorOccurred("logClearArrivalRequest", "LOG not connected");
            return;
        }

        UA_Boolean bFalse = false;

        QByteArray emptyBa;
        UA_String empty;
        empty.length = 0;
        empty.data = (UA_Byte*)emptyBa.data();

        UA_Variant vFalse, vEmpty;
        UA_Variant_setScalar(&vFalse, &bFalse, &UA_TYPES[UA_TYPES_BOOLEAN]);
        UA_Variant_setScalar(&vEmpty, &empty, &UA_TYPES[UA_TYPES_STRING]);

        UA_Client_writeValueAttribute(log.client, nid(LOG_ARRIVAL_PENDING), &vFalse);
        UA_Client_writeValueAttribute(log.client, nid(LOG_ARRIVAL_DONE), &vFalse);
        UA_Client_writeValueAttribute(log.client, nid(LOG_ARRIVAL_OK), &vFalse);
        UA_Client_writeValueAttribute(log.client, nid(LOG_ARRIVAL_MSG), &vEmpty);
        UA_Client_writeValueAttribute(log.client, nid(LOG_ARRIVAL_ORDER_ID), &vEmpty);
    }

    void mfgSendAuthResult(bool ok) {
        QMutexLocker lk(&mu);
        if(!mfg.client || !mfg.connected) {
            emit errorOccurred("mfgSendAuthResult", "MFG not connected");
            return;
        }

        UA_Boolean bOk = ok ? true : false;
        UA_Boolean bDone = true;
        UA_Boolean bPending = false;

        UA_Variant vOk, vDone, vPending;
        UA_Variant_setScalar(&vOk, &bOk, &UA_TYPES[UA_TYPES_BOOLEAN]);
        UA_Variant_setScalar(&vDone, &bDone, &UA_TYPES[UA_TYPES_BOOLEAN]);
        UA_Variant_setScalar(&vPending, &bPending, &UA_TYPES[UA_TYPES_BOOLEAN]);

        UA_Client_writeValueAttribute(mfg.client, nid(MFG_AUTH_RESULT_OK), &vOk);
        UA_Client_writeValueAttribute(mfg.client, nid(MFG_AUTH_RESULT_DONE), &vDone);
        UA_Client_writeValueAttribute(mfg.client, nid(MFG_AUTH_REQ_PENDING), &vPending);
    }

    void logSendAuthResult(bool ok) {
        QMutexLocker lk(&mu);
        if(!log.client || !log.connected) {
            emit errorOccurred("logSendAuthResult", "LOG not connected");
            return;
        }

        UA_Boolean bOk = ok ? true : false;
        UA_Boolean bDone = true;
        UA_Boolean bPending = false;

        UA_Variant vOk, vDone, vPending;
        UA_Variant_setScalar(&vOk, &bOk, &UA_TYPES[UA_TYPES_BOOLEAN]);
        UA_Variant_setScalar(&vDone, &bDone, &UA_TYPES[UA_TYPES_BOOLEAN]);
        UA_Variant_setScalar(&vPending, &bPending, &UA_TYPES[UA_TYPES_BOOLEAN]);

        UA_Client_writeValueAttribute(log.client, nid(LOG_AUTH_RESULT_OK), &vOk);
        UA_Client_writeValueAttribute(log.client, nid(LOG_AUTH_RESULT_DONE), &vDone);
        UA_Client_writeValueAttribute(log.client, nid(LOG_AUTH_REQ_PENDING), &vPending);
    }

private slots:
    void finalizeDropMfg() { finalizeDrop(mfg, true); }
    void finalizeDropLog() { finalizeDrop(log, false); }

signals:
    void mfgConnectedChanged(bool connected);
    void logConnectedChanged(bool connected);

    void mfgStatusUpdated(const QString &status);
    void mfgTempUpdated(double temp);
    void mfgHumUpdated(double hum);
    void mfgSpeedUpdated(double speed);
    void mfgProdCountUpdated(quint64 v);
    void mfgDefectCountUpdated(quint64 v);
    void mfgAttemptCountUpdated(quint64 v);
    void mfgDefectRateUpdated(double v);
    void mfgDefectCodeUpdated(quint16 v);

    void logStatusUpdated(const QString &status);
    void logTempUpdated(double temp);
    void logHumUpdated(double hum);
    void logSpeedUpdated(int idx1to3, double speed);

    void logWhLoadingUpdated(int wh1to3, bool loading);
    void logWhLoadedUpdated(int wh1to3, bool loaded);
    void logWhQtyUpdated(int wh1to3, quint32 qty);
    void logWhLowStockUpdated(int wh1to3, bool low);

    void mfgAuthRequestReceived(const QString &id, const QString &pw);
    void logAuthRequestReceived(const QString &id, const QString &pw);

    void logArrivalRequested(const QString &orderId);

    void errorOccurred(const QString &where, const QString &msg);
    void info(const QString &msg);

private:
    struct Conn {
        UA_Client *client = nullptr;
        UA_UInt32 subId = 0;

        QString endpoint;
        QString user;
        QString pass;
        QString clientCertPath;
        QString clientKeyPath;
        QString trustSrvPath;

        UA_ByteString clientCert = UA_BYTESTRING_NULL;
        UA_ByteString clientKey  = UA_BYTESTRING_NULL;
        UA_ByteString trustSrv   = UA_BYTESTRING_NULL;

        bool pendingDrop = false;
        bool connected = false;

        bool reconnectEnabled = false;
        qint64 lastRetryMs = 0;
        int retryIntervalMs = 3000;   // 3초마다 재시도

        QList<MonTag*> tags;
    };

    struct QuarantinedConn {
        UA_Client *client = nullptr;

        UA_ByteString clientCert = UA_BYTESTRING_NULL;
        UA_ByteString clientKey  = UA_BYTESTRING_NULL;
        UA_ByteString trustSrv   = UA_BYTESTRING_NULL;

        QList<MonTag*> tags;
    };

    QTimer *loopTimer = nullptr;
    QMutex mu;

    Conn mfg;
    Conn log;
    QList<QuarantinedConn> garbage;

    static qint64 nowMs() {
        return QDateTime::currentMSecsSinceEpoch();
    }

    static void moveByteString(UA_ByteString &dst, UA_ByteString &src) {
        dst = src;
        src = UA_BYTESTRING_NULL;
    }

    static void clearTagList(QList<MonTag*> &tags) {
        for(auto *t : tags) delete t;
        tags.clear();
    }

    static double clampPct(double v) {
        if(v < 0.0) return 0.0;
        if(v > 100.0) return 100.0;
        return v;
    }

    static QString readStringNode(UA_Client *client, const char *nodeId) {
        if(!client) return {};

        UA_Variant v;
        UA_Variant_init(&v);

        QString out;
        UA_StatusCode rc = UA_Client_readValueAttribute(client, nid(nodeId), &v);
        if(rc == UA_STATUSCODE_GOOD &&
            UA_Variant_hasScalarType(&v, &UA_TYPES[UA_TYPES_STRING])) {
            UA_String s = *static_cast<UA_String*>(v.data);
            out = QString::fromUtf8((const char*)s.data, (int)s.length);
        }

        UA_Variant_clear(&v);
        return out;
    }

    void emitMfgAuthRequest() {
        QTimer::singleShot(50, this, [this]() {
            QString id = readStringNode(mfg.client, MFG_AUTH_REQ_ID);
            QString pw = readStringNode(mfg.client, MFG_AUTH_REQ_PW);
            emit info(QString("[AUTH][MFG] delayed read id=[%1] pw=[%2]").arg(id, pw));
            emit mfgAuthRequestReceived(id, pw);
        });
    }

    void emitLogAuthRequest() {
        QTimer::singleShot(50, this, [this]() {
            QString id = readStringNode(log.client, LOG_AUTH_REQ_ID);
            QString pw = readStringNode(log.client, LOG_AUTH_REQ_PW);
            emit info(QString("[AUTH][LOG] delayed read id=[%1] pw=[%2]").arg(id, pw));
            emit logAuthRequestReceived(id, pw);
        });
    }

    void emitLogArrivalRequest() {
        QTimer::singleShot(50, this, [this]() {
            QString orderId = readStringNode(log.client, LOG_ARRIVAL_ORDER_ID);
            emit info(QString("[ARRIVAL][LOG] orderId=[%1]").arg(orderId));
            if(!orderId.isEmpty())
                emit logArrivalRequested(orderId);
        });
    }

    void quarantineConn(Conn &c, bool isMfg) {
        if(!c.client && !c.connected && c.tags.isEmpty())
            return;

        QuarantinedConn q;
        q.client = c.client;
        c.client = nullptr;

        moveByteString(q.clientCert, c.clientCert);
        moveByteString(q.clientKey,  c.clientKey);
        moveByteString(q.trustSrv,   c.trustSrv);

        q.tags = std::move(c.tags);

        c.subId = 0;
        c.pendingDrop = false;

        bool wasConnected = c.connected;
        c.connected = false;

        garbage.push_back(std::move(q));

        if(wasConnected) {
            if(isMfg) emit mfgConnectedChanged(false);
            else emit logConnectedChanged(false);
        }
    }
    void quarantineFailedClient(UA_Client *client,
                                UA_ByteString &clientCert,
                                UA_ByteString &clientKey,
                                UA_ByteString &trustSrv) {
        QuarantinedConn q;
        q.client = client;

        moveByteString(q.clientCert, clientCert);
        moveByteString(q.clientKey,  clientKey);
        moveByteString(q.trustSrv,   trustSrv);

        garbage.push_back(std::move(q));
    }

    void finalizeDrop(Conn &c, bool isMfg) {
        QMutexLocker lk(&mu);

        if(!c.pendingDrop)
            return;

        quarantineConn(c, isMfg);
    }

    // ---- subscription callback ----
    static void dataChangeCb(UA_Client * /*client*/,
                             UA_UInt32 /*subId*/, void *subContext,
                             UA_UInt32 /*monId*/, void *monContext,
                             UA_DataValue *value) {
        auto *self = static_cast<Worker*>(subContext);
        auto *tag  = static_cast<MonTag*>(monContext);
        if(!self || !tag || !value || !value->hasValue) return;

        // String
        if(UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_STRING])) {
            UA_String s = *static_cast<UA_String*>(value->value.data);
            QString qs = QString::fromUtf8((const char*)s.data, (int)s.length);

            if(tag->kind == TagKind::MFG_STATUS) emit self->mfgStatusUpdated(qs);
            else if(tag->kind == TagKind::LOG_STATUS) emit self->logStatusUpdated(qs);
            return;
        }


        // Double
        if(UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_DOUBLE])) {
            double v = *static_cast<double*>(value->value.data);
            switch(tag->kind) {
            case TagKind::MFG_TEMP: emit self->mfgTempUpdated(v); break;
            case TagKind::MFG_HUM: emit self->mfgHumUpdated(v); break;
            case TagKind::MFG_SPEED: emit self->mfgSpeedUpdated(v); break;
            case TagKind::MFG_DEFECT_RATE: emit self->mfgDefectRateUpdated(v); break;
            case TagKind::LOG_TEMP: emit self->logTempUpdated(v); break;
            case TagKind::LOG_HUM: emit self->logHumUpdated(v); break;
            case TagKind::LOG_SPEED1: emit self->logSpeedUpdated(1, v); break;
            case TagKind::LOG_SPEED2: emit self->logSpeedUpdated(2, v); break;
            case TagKind::LOG_SPEED3: emit self->logSpeedUpdated(3, v); break;
            default: break;
            }
            return;
        }

        // UInt64
        if(UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_UINT64])) {
            quint64 v = (quint64)(*static_cast<UA_UInt64*>(value->value.data));
            if(tag->kind == TagKind::MFG_PROD_COUNT) emit self->mfgProdCountUpdated(v);
            else if(tag->kind == TagKind::MFG_DEFECT_COUNT) emit self->mfgDefectCountUpdated(v);
            else if(tag->kind == TagKind::MFG_ATTEMPT_COUNT) emit self->mfgAttemptCountUpdated(v);
            return;
        }

        // UInt32
        if(UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_UINT32])) {
            quint32 v = (quint32)(*static_cast<UA_UInt32*>(value->value.data));
            if(tag->kind == TagKind::WH_QTY_1) emit self->logWhQtyUpdated(1, v);
            else if(tag->kind == TagKind::WH_QTY_2) emit self->logWhQtyUpdated(2, v);
            else if(tag->kind == TagKind::WH_QTY_3) emit self->logWhQtyUpdated(3, v);
            return;
        }

        // UInt16
        if(UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_UINT16])) {
            quint16 v = (quint16)(*static_cast<UA_UInt16*>(value->value.data));
            if(tag->kind == TagKind::MFG_DEFECT_CODE) emit self->mfgDefectCodeUpdated(v);
            return;
        }


        // Bool
        if(UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_BOOLEAN])) {
            bool b = (*static_cast<UA_Boolean*>(value->value.data)) ? true : false;

            if(tag->kind == TagKind::MFG_AUTH_PENDING) {
                if(b) self->emitMfgAuthRequest();
                return;
            }
            if(tag->kind == TagKind::LOG_AUTH_PENDING) {
                if(b) self->emitLogAuthRequest();
                return;
            }
            if(tag->kind == TagKind::LOG_ARRIVAL_PENDING) {
                if(b) self->emitLogArrivalRequest();
                return;
            }
            switch(tag->kind) {
            case TagKind::WH_LOADING_1: emit self->logWhLoadingUpdated(1, b); break;
            case TagKind::WH_LOADING_2: emit self->logWhLoadingUpdated(2, b); break;
            case TagKind::WH_LOADING_3: emit self->logWhLoadingUpdated(3, b); break;

            case TagKind::WH_LOADED_1:  emit self->logWhLoadedUpdated(1, b); break;
            case TagKind::WH_LOADED_2:  emit self->logWhLoadedUpdated(2, b); break;
            case TagKind::WH_LOADED_3:  emit self->logWhLoadedUpdated(3, b); break;

            case TagKind::WH_LOW_1:     emit self->logWhLowStockUpdated(1, b); break;
            case TagKind::WH_LOW_2:     emit self->logWhLowStockUpdated(2, b); break;
            case TagKind::WH_LOW_3:     emit self->logWhLowStockUpdated(3, b); break;
            default: break;
            }
            return;
        }
    }

    void iterate() {
        bool mfgLost = false;
        bool logLost = false;
        QString mfgErr, logErr;

        {
            QMutexLocker lk(&mu);

            if(mfg.client && !mfg.pendingDrop) {
                UA_StatusCode rc = UA_Client_run_iterate(mfg.client, 0);
                if(rc != UA_STATUSCODE_GOOD) {
                    mfgLost = true;
                    mfgErr = sc(rc);
                    mfg.pendingDrop = true;
                }
            }

            if(log.client && !log.pendingDrop) {
                UA_StatusCode rc = UA_Client_run_iterate(log.client, 0);
                if(rc != UA_STATUSCODE_GOOD) {
                    logLost = true;
                    logErr = sc(rc);
                    log.pendingDrop = true;
                }
            }
        }

        if(mfgLost) {
            emit errorOccurred("MFG", "server disconnected: " + mfgErr);
            QMetaObject::invokeMethod(this, "finalizeDropMfg", Qt::QueuedConnection);
        }

        if(logLost) {
            emit errorOccurred("LOG", "server disconnected: " + logErr);
            QMetaObject::invokeMethod(this, "finalizeDropLog", Qt::QueuedConnection);
        }

        // ✅ 자동 재접속
        {
            QMutexLocker lk(&mu);
            qint64 now = nowMs();

            if(!mfg.client && !mfg.connected && mfg.reconnectEnabled) {
                if(now - mfg.lastRetryMs >= mfg.retryIntervalMs) {
                    mfg.lastRetryMs = now;
                    emit info("Retrying MFG connection...");
                    connectOne(mfg,
                               mfg.endpoint, mfg.user, mfg.pass,
                               mfg.clientCertPath, mfg.clientKeyPath, mfg.trustSrvPath,
                               true);
                }
            }

            if(!log.client && !log.connected && log.reconnectEnabled) {
                if(now - log.lastRetryMs >= log.retryIntervalMs) {
                    log.lastRetryMs = now;
                    emit info("Retrying LOG connection...");
                    connectOne(log,
                               log.endpoint, log.user, log.pass,
                               log.clientCertPath, log.clientKeyPath, log.trustSrvPath,
                               false);
                }
            }
        }
    }

    void cleanup() {
        disconnectOneGraceful(mfg, true);
        disconnectOneGraceful(log, false);

        for(auto &q : garbage) {
            UA_ByteString_clear(&q.clientCert);
            UA_ByteString_clear(&q.clientKey);
            UA_ByteString_clear(&q.trustSrv);
            clearTagList(q.tags);
            q.client = nullptr;
        }
        garbage.clear();
    }

    void disconnectOneGraceful(Conn &c, bool isMfg) {
        UA_Client *client = c.client;
        c.client = nullptr;
        c.subId = 0;
        c.pendingDrop = false;

        bool wasConnected = c.connected;
        c.connected = false;

        if(client) {
            UA_Client_disconnect(client);
            UA_Client_delete(client);
        }

        UA_ByteString_clear(&c.clientCert);
        UA_ByteString_clear(&c.clientKey);
        UA_ByteString_clear(&c.trustSrv);
        clearTagList(c.tags);

        if(wasConnected) {
            if(isMfg) emit mfgConnectedChanged(false);
            else emit logConnectedChanged(false);
        }
    }

    void connectOne(Conn &c,
                    const QString &endpoint,
                    const QString &user, const QString &pass,
                    const QString &clientCertDer,
                    const QString &clientKeyDer,
                    const QString &trustServerDer,
                    bool isMfg) {
        // 기존 연결 정리
        disconnectOneGraceful(c, isMfg);

        UA_Client *client = UA_Client_new();
        if(!client) {
            emit errorOccurred(isMfg ? "connectMfg" : "connectLog", "UA_Client_new failed");
            c.connected = false;
            c.pendingDrop = false;
            c.client = nullptr;
            c.subId = 0;
            if(isMfg) emit mfgConnectedChanged(false);
            else emit logConnectedChanged(false);
            return;
        }

        UA_ClientConfig *cfg = UA_Client_getConfig(client);
        UA_ClientConfig_setDefault(cfg);

        // 인증서/키/신뢰 서버 인증서 로드
        c.clientCert = loadFileBytes(clientCertDer);
        c.clientKey  = loadFileBytes(clientKeyDer);
        c.trustSrv   = loadFileBytes(trustServerDer);

        emit info(QString("cert/key/trust len = %1/%2/%3")
                      .arg((int)c.clientCert.length)
                      .arg((int)c.clientKey.length)
                      .arg((int)c.trustSrv.length));

        if(c.clientCert.length == 0 || c.clientKey.length == 0 || c.trustSrv.length == 0) {
            emit errorOccurred(isMfg ? "connectMfg" : "connectLog", "cert/key/trust load failed");

            quarantineFailedClient(client, c.clientCert, c.clientKey, c.trustSrv);

            c.connected = false;
            c.pendingDrop = false;
            c.client = nullptr;
            c.subId = 0;
            if(isMfg) emit mfgConnectedChanged(false);
            else emit logConnectedChanged(false);
            return;
        }

        const UA_ByteString trustList[1] = { c.trustSrv };

        UA_StatusCode rcEnc = UA_ClientConfig_setDefaultEncryption(
            cfg,
            c.clientCert, c.clientKey,
            trustList, 1,
            NULL, 0
            );

        // 인증서 URI와 맞춰야 함
        cfg->clientDescription.applicationUri = UA_STRING_ALLOC("urn:opcua:mes");

        emit info(QString("setDefaultEncryption rc=%1").arg(sc(rcEnc)));

        if(rcEnc != UA_STATUSCODE_GOOD) {
            emit errorOccurred(isMfg ? "connectMfg" : "connectLog",
                               "setDefaultEncryption failed: " + sc(rcEnc));

            quarantineFailedClient(client, c.clientCert, c.clientKey, c.trustSrv);

            c.connected = false;
            c.pendingDrop = false;
            c.client = nullptr;
            c.subId = 0;

            if(isMfg) emit mfgConnectedChanged(false);
            else emit logConnectedChanged(false);
            return;
        }

        // 서버 강제 보안 모드/정책
        cfg->securityMode = UA_MESSAGESECURITYMODE_SIGNANDENCRYPT;
        cfg->securityPolicyUri = UA_STRING("http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256");

        // 실제 연결
        UA_StatusCode rc = UA_Client_connectUsername(
            client,
            endpoint.toUtf8().constData(),
            user.toUtf8().constData(),
            pass.toUtf8().constData()
            );

        emit info(QString("connectUsername rc=%1").arg(sc(rc)));

        if(rc != UA_STATUSCODE_GOOD) {
            emit errorOccurred(isMfg ? "connectMfg" : "connectLog",
                               "connect failed: " + sc(rc));

            quarantineFailedClient(client, c.clientCert, c.clientKey, c.trustSrv);

            c.connected = false;
            c.pendingDrop = false;
            c.client = nullptr;
            c.subId = 0;

            if(isMfg) emit mfgConnectedChanged(false);
            else emit logConnectedChanged(false);
            return;
        }

        // 연결 성공
        c.client = client;
        c.endpoint = endpoint;
        c.pendingDrop = false;
        c.connected = true;

        if(isMfg) emit mfgConnectedChanged(true);
        else emit logConnectedChanged(true);

        emit info(QString("%1 connected: %2").arg(isMfg ? "MFG" : "LOG").arg(endpoint));

        setupSubscriptions(c, isMfg);
    }


    void setupSubscriptions(Conn &c, bool isMfg) {
        if(!c.client) return;

        UA_CreateSubscriptionRequest req = UA_CreateSubscriptionRequest_default();
        UA_CreateSubscriptionResponse resp =
            UA_Client_Subscriptions_create(c.client, req, this, nullptr, nullptr);

        if(resp.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
            emit errorOccurred(isMfg ? "subCreate(MFG)" : "subCreate(LOG)",
                               "create subscription failed: " + sc(resp.responseHeader.serviceResult));
            return;
        }

        c.subId = resp.subscriptionId;

        if(isMfg) {
            addMon(c, nid(MFG_STATUS),        TagKind::MFG_STATUS);
            addMon(c, nid(MFG_TEMP),          TagKind::MFG_TEMP);
            addMon(c, nid(MFG_HUM),           TagKind::MFG_HUM);
            addMon(c, nid(MFG_SPEED),         TagKind::MFG_SPEED);
            addMon(c, nid(MFG_PROD_COUNT),    TagKind::MFG_PROD_COUNT);
            addMon(c, nid(MFG_DEFECT_COUNT),  TagKind::MFG_DEFECT_COUNT);
            addMon(c, nid(MFG_ATTEMPT_COUNT), TagKind::MFG_ATTEMPT_COUNT);
            addMon(c, nid(MFG_DEFECT_RATE),   TagKind::MFG_DEFECT_RATE);
            addMon(c, nid(MFG_DEFECT_CODE),   TagKind::MFG_DEFECT_CODE);
            addMon(c, nid(MFG_AUTH_REQ_PENDING), TagKind::MFG_AUTH_PENDING);
        } else {
            addMon(c, nid(LOG_STATUS), TagKind::LOG_STATUS);
            addMon(c, nid(LOG_TEMP),   TagKind::LOG_TEMP);
            addMon(c, nid(LOG_HUM),    TagKind::LOG_HUM);

            addMon(c, nid(LOG_SPEED1), TagKind::LOG_SPEED1);
            addMon(c, nid(LOG_SPEED2), TagKind::LOG_SPEED2);
            addMon(c, nid(LOG_SPEED3), TagKind::LOG_SPEED3);

            addMon(c, nid(LOG_WH_LOADING[0]), TagKind::WH_LOADING_1);
            addMon(c, nid(LOG_WH_LOADING[1]), TagKind::WH_LOADING_2);
            addMon(c, nid(LOG_WH_LOADING[2]), TagKind::WH_LOADING_3);

            addMon(c, nid(LOG_WH_LOADED[0]),  TagKind::WH_LOADED_1);
            addMon(c, nid(LOG_WH_LOADED[1]),  TagKind::WH_LOADED_2);
            addMon(c, nid(LOG_WH_LOADED[2]),  TagKind::WH_LOADED_3);

            addMon(c, nid(LOG_WH_QTY[0]),     TagKind::WH_QTY_1);
            addMon(c, nid(LOG_WH_QTY[1]),     TagKind::WH_QTY_2);
            addMon(c, nid(LOG_WH_QTY[2]),     TagKind::WH_QTY_3);

            addMon(c, nid(LOG_WH_LOW[0]),     TagKind::WH_LOW_1);
            addMon(c, nid(LOG_WH_LOW[1]),     TagKind::WH_LOW_2);
            addMon(c, nid(LOG_WH_LOW[2]),     TagKind::WH_LOW_3);
            addMon(c, nid(LOG_AUTH_REQ_PENDING), TagKind::LOG_AUTH_PENDING);
            addMon(c, nid(LOG_ARRIVAL_PENDING), TagKind::LOG_ARRIVAL_PENDING);
        }
    }

    void addMon(Conn &c, const UA_NodeId &node, TagKind kind) {
        if(!c.client || c.subId == 0) return;

        auto *tag = new MonTag{kind};
        c.tags.push_back(tag);

        UA_MonitoredItemCreateRequest monReq = UA_MonitoredItemCreateRequest_default(node);
        monReq.requestedParameters.samplingInterval = 200.0;

        UA_MonitoredItemCreateResult monRes =
            UA_Client_MonitoredItems_createDataChange(
                c.client,
                c.subId,
                UA_TIMESTAMPSTORETURN_SOURCE,
                monReq,
                tag,
                dataChangeCb,
                nullptr
                );

        if(monRes.statusCode != UA_STATUSCODE_GOOD) {
            emit errorOccurred("addMon", "create monitored item failed: " + sc(monRes.statusCode));
        }
    }
};

// ---------------- OpcUaService facade ----------------
OpcUaService::OpcUaService(QObject *parent) : QObject(parent) {
    m_worker = new Worker();
    m_worker->moveToThread(&m_thread);

    connect(m_worker, &Worker::mfgConnectedChanged, this, &OpcUaService::mfgConnectedChanged);
    connect(m_worker, &Worker::logConnectedChanged, this, &OpcUaService::logConnectedChanged);

    connect(m_worker, &Worker::mfgStatusUpdated, this, &OpcUaService::mfgStatusUpdated);
    connect(m_worker, &Worker::mfgTempUpdated, this, &OpcUaService::mfgTempUpdated);
    connect(m_worker, &Worker::mfgHumUpdated, this, &OpcUaService::mfgHumUpdated);
    connect(m_worker, &Worker::mfgSpeedUpdated, this, &OpcUaService::mfgSpeedUpdated);
    connect(m_worker, &Worker::mfgProdCountUpdated, this, &OpcUaService::mfgProdCountUpdated);
    connect(m_worker, &Worker::mfgDefectCountUpdated, this, &OpcUaService::mfgDefectCountUpdated);
    connect(m_worker, &Worker::mfgAttemptCountUpdated, this, &OpcUaService::mfgAttemptCountUpdated);
    connect(m_worker, &Worker::mfgDefectRateUpdated, this, &OpcUaService::mfgDefectRateUpdated);
    connect(m_worker, &Worker::mfgDefectCodeUpdated, this, &OpcUaService::mfgDefectCodeUpdated);

    connect(m_worker, &Worker::logStatusUpdated, this, &OpcUaService::logStatusUpdated);
    connect(m_worker, &Worker::logTempUpdated, this, &OpcUaService::logTempUpdated);
    connect(m_worker, &Worker::logHumUpdated, this, &OpcUaService::logHumUpdated);
    connect(m_worker, &Worker::logSpeedUpdated, this, &OpcUaService::logSpeedUpdated);

    connect(m_worker, &Worker::logWhLoadingUpdated, this, &OpcUaService::logWhLoadingUpdated);
    connect(m_worker, &Worker::logWhLoadedUpdated, this, &OpcUaService::logWhLoadedUpdated);
    connect(m_worker, &Worker::logWhQtyUpdated, this, &OpcUaService::logWhQtyUpdated);
    connect(m_worker, &Worker::logWhLowStockUpdated, this, &OpcUaService::logWhLowStockUpdated);

    connect(m_worker, &Worker::mfgAuthRequestReceived, this, &OpcUaService::mfgAuthRequestReceived);
    connect(m_worker, &Worker::logAuthRequestReceived, this, &OpcUaService::logAuthRequestReceived);

    connect(m_worker, &Worker::errorOccurred, this, &OpcUaService::errorOccurred);
    connect(m_worker, &Worker::info, this, &OpcUaService::info);

    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &Worker::logArrivalRequested, this, &OpcUaService::logArrivalRequested);
}

OpcUaService::~OpcUaService() {
    stop();
}

void OpcUaService::start() {
    if(m_thread.isRunning()) return;
    m_thread.start();
    QMetaObject::invokeMethod(m_worker, "startLoop", Qt::QueuedConnection);
}

void OpcUaService::stop() {
    if(!m_thread.isRunning()) return;
    QMetaObject::invokeMethod(m_worker, "stopLoop", Qt::QueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

void OpcUaService::connectMfg(const QString &endpoint, const QString &user, const QString &pass,
                              const QString &clientCertDer, const QString &clientKeyDer, const QString &trustServerDer) {
    start();
    QMetaObject::invokeMethod(m_worker, "connectMfg", Qt::QueuedConnection,
                              Q_ARG(QString, endpoint),
                              Q_ARG(QString, user),
                              Q_ARG(QString, pass),
                              Q_ARG(QString, clientCertDer),
                              Q_ARG(QString, clientKeyDer),
                              Q_ARG(QString, trustServerDer));
}

void OpcUaService::connectLog(const QString &endpoint, const QString &user, const QString &pass,
                              const QString &clientCertDer, const QString &clientKeyDer, const QString &trustServerDer) {
    start();
    QMetaObject::invokeMethod(m_worker, "connectLog", Qt::QueuedConnection,
                              Q_ARG(QString, endpoint),
                              Q_ARG(QString, user),
                              Q_ARG(QString, pass),
                              Q_ARG(QString, clientCertDer),
                              Q_ARG(QString, clientKeyDer),
                              Q_ARG(QString, trustServerDer));
}

void OpcUaService::disconnectAll() {
    if(!m_thread.isRunning()) return;
    QMetaObject::invokeMethod(m_worker, "disconnectAll", Qt::QueuedConnection);
}

// ---- MFG ----
void OpcUaService::mfgWriteSpeed(double speedPercent) {
    start();
    QMetaObject::invokeMethod(m_worker, "mfgWriteSpeed", Qt::QueuedConnection,
                              Q_ARG(double, speedPercent));
}

void OpcUaService::mfgStartOrder(const QString &orderId, quint16 productNo, quint32 qty) {
    start();
    QMetaObject::invokeMethod(m_worker, "mfgStartOrder", Qt::QueuedConnection,
                              Q_ARG(QString, orderId),
                              Q_ARG(quint16, productNo),
                              Q_ARG(quint32, qty));
}

void OpcUaService::mfgStopOrder() {
    start();
    QMetaObject::invokeMethod(m_worker, "mfgStopOrder", Qt::QueuedConnection);
}

// ---- LOG ----
void OpcUaService::logWriteSpeed(int idx1to3, double speedPercent) {
    start();
    QMetaObject::invokeMethod(m_worker, "logWriteSpeed", Qt::QueuedConnection,
                              Q_ARG(int, idx1to3),
                              Q_ARG(double, speedPercent));
}

void OpcUaService::logMove(int wh1to3, quint32 qty) {
    start();
    QMetaObject::invokeMethod(m_worker, "logMove", Qt::QueuedConnection,
                              Q_ARG(int, wh1to3),
                              Q_ARG(quint32, qty));
}

void OpcUaService::logStopMove() {
    start();
    QMetaObject::invokeMethod(m_worker, "logStopMove", Qt::QueuedConnection);
}

void OpcUaService::logConsume(int wh1to3, quint32 qty) {
    start();
    QMetaObject::invokeMethod(m_worker, "logConsume", Qt::QueuedConnection,
                              Q_ARG(int, wh1to3),
                              Q_ARG(quint32, qty));
}

void OpcUaService::logInitStocks(quint32 wh1Qty, quint32 wh2Qty, quint32 wh3Qty) {
    start();
    QMetaObject::invokeMethod(m_worker, "logInitStocks", Qt::QueuedConnection,
                              Q_ARG(quint32, wh1Qty),
                              Q_ARG(quint32, wh2Qty),
                              Q_ARG(quint32, wh3Qty));
}

void OpcUaService::mfgSendAuthResult(bool ok) {
    start();
    QMetaObject::invokeMethod(m_worker, "mfgSendAuthResult", Qt::QueuedConnection,
                              Q_ARG(bool, ok));
}

void OpcUaService::logSendAuthResult(bool ok) {
    start();
    QMetaObject::invokeMethod(m_worker, "logSendAuthResult", Qt::QueuedConnection,
                              Q_ARG(bool, ok));
}
void OpcUaService::logWriteArrivalResult(bool ok, const QString &msg) {
    start();
    QMetaObject::invokeMethod(m_worker, "logWriteArrivalResult", Qt::QueuedConnection,
                              Q_ARG(bool, ok),
                              Q_ARG(QString, msg));
}

void OpcUaService::logClearArrivalRequest() {
    start();
    QMetaObject::invokeMethod(m_worker, "logClearArrivalRequest", Qt::QueuedConnection);
}

#include "opcua_service.moc"
