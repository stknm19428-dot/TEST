#include "opcua_service.h"

#include <QMetaObject>
#include <QFile>

static UA_ByteString loadFileBytes(const QString &path) {
    UA_ByteString bs = UA_BYTESTRING_NULL;
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly)) return bs;
    QByteArray data = f.readAll();
    bs.length = (size_t)data.size();
    bs.data = (UA_Byte*)UA_malloc(bs.length);
    if(!bs.data) { bs.length = 0; return bs; }
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

// ---------- NodeId helpers (server code 그대로 반영) ----------
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
static const char *LOG_CONSUME_M       = "log/Consume";

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
        connectOne(mfg, endpoint, user, pass, clientCertDer, clientKeyDer, trustServerDer, true);
    }

    void connectLog(QString endpoint, QString user, QString pass,
                    QString clientCertDer, QString clientKeyDer, QString trustServerDer) {
        QMutexLocker lk(&mu);
        connectOne(log, endpoint, user, pass, clientCertDer, clientKeyDer, trustServerDer, false);
    }

    void disconnectAll() {
        QMutexLocker lk(&mu);
        disconnectOne(mfg, true);
        disconnectOne(log, false);
    }

    // ---- MFG API ----
    void mfgWriteSpeed(double pct) {
        QMutexLocker lk(&mu);
        if(!mfg.client) { emit errorOccurred("mfgWriteSpeed", "MFG not connected"); return; }
        double v = clampPct(pct);
        UA_Variant var; UA_Variant_setScalar(&var, &v, &UA_TYPES[UA_TYPES_DOUBLE]);
        UA_StatusCode rc = UA_Client_writeValueAttribute(mfg.client, nid(MFG_SPEED), &var);
        if(rc != UA_STATUSCODE_GOOD) emit errorOccurred("mfgWriteSpeed", "write failed: " + sc(rc));
    }

    void mfgStartOrder(QString orderId) {
        QMutexLocker lk(&mu);
        if(!mfg.client) { emit errorOccurred("mfgStartOrder", "MFG not connected"); return; }

        QByteArray ba = orderId.toUtf8();
        UA_String s; s.length = (size_t)ba.size(); s.data = (UA_Byte*)ba.data();

        UA_Variant in; UA_Variant_init(&in);
        UA_Variant_setScalar(&in, &s, &UA_TYPES[UA_TYPES_STRING]);

        UA_Variant out; UA_Variant_init(&out);
        UA_StatusCode rc = UA_Client_call(mfg.client,
                                          UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                          nid(MFG_STARTORDER_M),
                                          1, &in,
                                          0, nullptr);
        if(rc != UA_STATUSCODE_GOOD) emit errorOccurred("mfgStartOrder", "call failed: " + sc(rc));
    }

    void mfgStopOrder() {
        QMutexLocker lk(&mu);
        if(!mfg.client) { emit errorOccurred("mfgStopOrder", "MFG not connected"); return; }

        UA_StatusCode rc = UA_Client_call(mfg.client,
                                          UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                          nid(MFG_STOPORDER_M),
                                          0, nullptr,
                                          0, nullptr);
        if(rc != UA_STATUSCODE_GOOD) emit errorOccurred("mfgStopOrder", "call failed: " + sc(rc));
    }

    // ---- LOG API ----
    void logWriteSpeed(int idx1to3, double pct) {
        QMutexLocker lk(&mu);
        if(!log.client) { emit errorOccurred("logWriteSpeed", "LOG not connected"); return; }
        if(idx1to3 < 1 || idx1to3 > 3) { emit errorOccurred("logWriteSpeed", "idx out of range"); return; }

        const char *node = (idx1to3==1)?LOG_SPEED1:(idx1to3==2)?LOG_SPEED2:LOG_SPEED3;
        double v = clampPct(pct);
        UA_Variant var; UA_Variant_setScalar(&var, &v, &UA_TYPES[UA_TYPES_DOUBLE]);
        UA_StatusCode rc = UA_Client_writeValueAttribute(log.client, nid(node), &var);
        if(rc != UA_STATUSCODE_GOOD) emit errorOccurred("logWriteSpeed", "write failed: " + sc(rc));
    }

    void logMove(int wh1to3, quint32 qty) {
        QMutexLocker lk(&mu);
        if(!log.client) { emit errorOccurred("logMove", "LOG not connected"); return; }
        if(wh1to3 < 1 || wh1to3 > 3) { emit errorOccurred("logMove", "wh out of range"); return; }

        UA_UInt16 wh = (UA_UInt16)wh1to3;
        UA_UInt32 q = (UA_UInt32)qty;

        UA_Variant in[2];
        UA_Variant_init(&in[0]); UA_Variant_init(&in[1]);
        UA_Variant_setScalar(&in[0], &wh, &UA_TYPES[UA_TYPES_UINT16]);
        UA_Variant_setScalar(&in[1], &q,  &UA_TYPES[UA_TYPES_UINT32]);

        UA_StatusCode rc = UA_Client_call(log.client,
                                          UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                          nid(LOG_MOVE_M),
                                          2, in,
                                          0, nullptr);
        if(rc != UA_STATUSCODE_GOOD) emit errorOccurred("logMove", "call failed: " + sc(rc));
    }

    void logStopMove() {
        QMutexLocker lk(&mu);
        if(!log.client) { emit errorOccurred("logStopMove", "LOG not connected"); return; }

        UA_StatusCode rc = UA_Client_call(log.client,
                                          UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                          nid(LOG_STOPMOVE_M),
                                          0, nullptr,
                                          0, nullptr);
        if(rc != UA_STATUSCODE_GOOD) emit errorOccurred("logStopMove", "call failed: " + sc(rc));
    }

    void logConsume(int wh1to3, quint32 qty) {
        QMutexLocker lk(&mu);
        if(!log.client) { emit errorOccurred("logConsume", "LOG not connected"); return; }
        if(wh1to3 < 1 || wh1to3 > 3) { emit errorOccurred("logConsume", "wh out of range"); return; }

        UA_UInt16 wh = (UA_UInt16)wh1to3;
        UA_UInt32 q = (UA_UInt32)qty;

        UA_Variant in[2];
        UA_Variant_init(&in[0]); UA_Variant_init(&in[1]);
        UA_Variant_setScalar(&in[0], &wh, &UA_TYPES[UA_TYPES_UINT16]);
        UA_Variant_setScalar(&in[1], &q,  &UA_TYPES[UA_TYPES_UINT32]);

        UA_StatusCode rc = UA_Client_call(log.client,
                                          UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                          nid(LOG_CONSUME_M),
                                          2, in,
                                          0, nullptr);
        if(rc != UA_STATUSCODE_GOOD) emit errorOccurred("logConsume", "call failed: " + sc(rc));
    }

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

    void errorOccurred(const QString &where, const QString &msg);
    void info(const QString &msg);

private:
    struct Conn {
        UA_Client *client = nullptr;
        UA_UInt32 subId = 0;
        QString endpoint;

        // ✅ handshake 동안 살아있어야 함
        UA_ByteString clientCert = UA_BYTESTRING_NULL;
        UA_ByteString clientKey  = UA_BYTESTRING_NULL;
        UA_ByteString trustSrv   = UA_BYTESTRING_NULL;
    };

    QTimer *loopTimer = nullptr;
    QMutex mu;

    Conn mfg;
    Conn log;

    QList<MonTag*> tagsToFree; // monContext에 할당한 tag들 정리

    static double clampPct(double v) {
        if(v < 0.0) return 0.0;
        if(v > 100.0) return 100.0;
        return v;
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
        QMutexLocker lk(&mu);
        if(mfg.client) UA_Client_run_iterate(mfg.client, 0);
        if(log.client) UA_Client_run_iterate(log.client, 0);
    }

    void cleanup() {
        for(auto *t : tagsToFree) delete t;
        tagsToFree.clear();

        disconnectOne(mfg, true);
        disconnectOne(log, false);
    }

    void disconnectOne(Conn &c, bool isMfg) {
        if(!c.client) return;
        UA_Client_disconnect(c.client);
        UA_Client_delete(c.client);
        c.client = nullptr;
        c.subId = 0;

        UA_ByteString_clear(&c.clientCert);
        UA_ByteString_clear(&c.clientKey);
        UA_ByteString_clear(&c.trustSrv);
        if(isMfg) emit mfgConnectedChanged(false);
        else emit logConnectedChanged(false);
    }

    void connectOne(Conn &c,
                    const QString &endpoint,
                    const QString &user, const QString &pass,
                    const QString &clientCertDer,
                    const QString &clientKeyDer,
                    const QString &trustServerDer,
                    bool isMfg) {
        // 기존 연결 제거 (여기서 기존 ByteString들도 clear됨)
        disconnectOne(c, isMfg);

        UA_Client *client = UA_Client_new();
        if(!client) {
            emit errorOccurred(isMfg?"connectMfg":"connectLog", "UA_Client_new failed");
            return;
        }

        UA_ClientConfig *cfg = UA_Client_getConfig(client);
        UA_ClientConfig_setDefault(cfg);

        // ✅ Conn에 보관 (핸드셰이크 동안 살아있게)
        c.clientCert = loadFileBytes(clientCertDer);
        c.clientKey  = loadFileBytes(clientKeyDer);
        c.trustSrv   = loadFileBytes(trustServerDer);

        emit info(QString("cert/key/trust len = %1/%2/%3")
                      .arg((int)c.clientCert.length)
                      .arg((int)c.clientKey.length)
                      .arg((int)c.trustSrv.length));

        if(c.clientCert.length == 0 || c.clientKey.length == 0 || c.trustSrv.length == 0) {
            emit errorOccurred(isMfg?"connectMfg":"connectLog", "cert/key/trust load failed");
            UA_Client_delete(client);
            UA_ByteString_clear(&c.clientCert);
            UA_ByteString_clear(&c.clientKey);
            UA_ByteString_clear(&c.trustSrv);
            return;
        }

        const UA_ByteString trustList[1] = { c.trustSrv };

        UA_StatusCode rcEnc = UA_ClientConfig_setDefaultEncryption(
            cfg,
            c.clientCert, c.clientKey,
            trustList, 1,
            NULL, 0
            );
        // ★ 인증서 URI와 동일하게 맞춰야 서버가 CreateSession을 허용함
        cfg->clientDescription.applicationUri = UA_STRING_ALLOC("urn:opcua:mes");
        emit info(QString("setDefaultEncryption rc=%1").arg(sc(rcEnc)));

        if(rcEnc != UA_STATUSCODE_GOOD) {
            emit errorOccurred(isMfg?"connectMfg":"connectLog",
                               "setDefaultEncryption failed: " + sc(rcEnc));
            UA_Client_delete(client);
            UA_ByteString_clear(&c.clientCert);
            UA_ByteString_clear(&c.clientKey);
            UA_ByteString_clear(&c.trustSrv);
            return;
        }

        // ✅ 서버 강제 모드/정책 (ALLOC/clear 금지)
        cfg->securityMode = UA_MESSAGESECURITYMODE_SIGNANDENCRYPT;
        cfg->securityPolicyUri = UA_STRING("http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256");

        // ✅ connect
        UA_StatusCode rc = UA_Client_connectUsername(
            client,
            endpoint.toUtf8().constData(),
            user.toUtf8().constData(),
            pass.toUtf8().constData()
            );
        emit info(QString("connectUsername rc=%1").arg(sc(rc)));

        if(rc != UA_STATUSCODE_GOOD) {
            emit errorOccurred(isMfg?"connectMfg":"connectLog", "connect failed: " + sc(rc));
            UA_Client_delete(client);
            // 실패 시도 시 보관했던 것들 정리
            UA_ByteString_clear(&c.clientCert);
            UA_ByteString_clear(&c.clientKey);
            UA_ByteString_clear(&c.trustSrv);
            return;
        }

        // 연결 성공
        c.client = client;
        c.endpoint = endpoint;

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
            emit errorOccurred(isMfg?"subCreate(MFG)":"subCreate(LOG)",
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
        }
    }

    void addMon(Conn &c, const UA_NodeId &node, TagKind kind) {
        if(!c.client || c.subId == 0) return;

        auto *tag = new MonTag{kind};
        tagsToFree.push_back(tag);

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
                nullptr);

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

    connect(m_worker, &Worker::errorOccurred, this, &OpcUaService::errorOccurred);
    connect(m_worker, &Worker::info, this, &OpcUaService::info);

    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
}

OpcUaService::~OpcUaService() { stop(); }

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
void OpcUaService::mfgStartOrder(const QString &orderId) {
    start();
    QMetaObject::invokeMethod(m_worker, "mfgStartOrder", Qt::QueuedConnection,
                              Q_ARG(QString, orderId));
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

#include "opcua_service.moc"
