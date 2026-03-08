// mes_client_log_wh_final.c  (open62541 v1.5.2)
//
// - Secure client (Basic256Sha256 + SignAndEncrypt, trusted mfg/log certs)
// - Connect:
//    MFG: opc.tcp://10.10.16.208:4850
//    LOG: opc.tcp://localhost:4841
//
// - Key controls:
//    1 : MFG start (write mfg speed 75% then StartOrder)
//    m : MFG stop  (write mfg speed 0% then StopOrder)
//    7 : LOG set speeds (60/50/40)
//    a : LOG Move WH1 qty=10
//    s : LOG Move WH2 qty=20
//    d : LOG Move WH3 qty=30
//    x : LOG StopMove()
//    q : stop all (MFG stop + LOG stop)
//
// - Subscriptions:
//    MFG: temp/hum + production nodes (optional output)
//    LOG: WH1~3 loading/loaded/load_qty
//
// Build:
//  gcc -Wall -Wextra -O2 mes_client_log_wh_final.c -o mes_client \
//    -I/usr/local/include -L/usr/local/lib -lopen62541 \
//    -lssl -lcrypto -lpthread -lm -lrt

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <unistd.h>
#include <sys/select.h>

#include <open62541/types.h>
#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/client_subscriptions.h>

/* ----------------- key input ----------------- */
static int stdin_has_key(void) {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO+1, &fds, NULL, NULL, &tv) > 0;
}
static int read_key_nonblock(void) {
    if(!stdin_has_key()) return -1;
    char c;
    if(read(STDIN_FILENO, &c, 1) == 1) return (int)c;
    return -1;
}
/*=============client==========================*/
static UA_Client *g_mfg_client = NULL;
static UA_Client *g_log_client = NULL;   /* LOG stop 보내려고 */

/* ----------------- graceful shutdown ----------------- */
static volatile sig_atomic_t g_running = 1;
static void on_sigint(int sig){ (void)sig; g_running = 0; }

/* ----------------- robust file loader ----------------- */
static UA_ByteString loadFile(const char *path) {
    UA_ByteString bs = UA_BYTESTRING_NULL;
    FILE *f = fopen(path, "rb");
    if(!f) {
        fprintf(stderr, "[MES] cannot open file: %s\n", path);
        return bs;
    }
    if(fseek(f, 0, SEEK_END) != 0) { fclose(f); return bs; }
    long sz = ftell(f);
    if(sz <= 0) { fclose(f); return bs; }
    rewind(f);

    bs.data = (UA_Byte*)UA_malloc((size_t)sz);
    if(!bs.data) { fclose(f); return UA_BYTESTRING_NULL; }
    bs.length = (size_t)sz;

    size_t n = fread(bs.data, 1, (size_t)sz, f);
    fclose(f);
    if(n != (size_t)sz) {
        UA_ByteString_clear(&bs);
        return UA_BYTESTRING_NULL;
    }
    return bs;
}

/* ----------------- PKI lifetime: global ----------------- */
static UA_ByteString g_clientCert = {0, NULL};
static UA_ByteString g_clientKey  = {0, NULL};
static UA_ByteString g_trustMfg   = {0, NULL};
static UA_ByteString g_trustLog   = {0, NULL};
static UA_Boolean    g_pki_loaded = UA_FALSE;

static UA_Boolean load_pki_once(void) {
    if(g_pki_loaded) return UA_TRUE;

    g_clientCert = loadFile("/home/pi/opcua_demo/certs/mes/cert.der");
    g_clientKey  = loadFile("/home/pi/opcua_demo/certs/mes/key.der");
    g_trustMfg   = loadFile("/home/pi/opcua_demo/certs/mes/trust_mfg.der");
    g_trustLog   = loadFile("/home/pi/opcua_demo/certs/mes/trust_log.der");

    g_pki_loaded = (g_clientCert.length > 0 &&
                    g_clientKey.length  > 0 &&
                    g_trustMfg.length   > 0 &&
                    g_trustLog.length   > 0) ? UA_TRUE : UA_FALSE;

    if(!g_pki_loaded) {
        fprintf(stderr, "[MES] PKI load failed (check paths)\n");
        UA_ByteString_clear(&g_clientCert);
        UA_ByteString_clear(&g_clientKey);
        UA_ByteString_clear(&g_trustMfg);
        UA_ByteString_clear(&g_trustLog);
    }
    return g_pki_loaded;
}

static void clear_pki(void) {
    UA_ByteString_clear(&g_clientCert);
    UA_ByteString_clear(&g_clientKey);
    UA_ByteString_clear(&g_trustMfg);
    UA_ByteString_clear(&g_trustLog);
    g_pki_loaded = UA_FALSE;
}

/* ----------------- helpers ----------------- */
static void freeCallOutput(size_t outSize, UA_Variant *out) {
    if(!out || outSize == 0) return;
    UA_Array_delete(out, outSize, &UA_TYPES[UA_TYPES_VARIANT]);
}

static UA_Client* makeSecureClient(void) {
    if(!load_pki_once())
        return NULL;

    UA_Client *client = UA_Client_new();
    UA_ClientConfig *cc = UA_Client_getConfig(client);

    UA_StatusCode rc = UA_ClientConfig_setDefault(cc);
    if(rc != UA_STATUSCODE_GOOD) {
        printf("[MES] Client default config failed: 0x%08x\n", (unsigned)rc);
        UA_Client_delete(client);
        return NULL;
    }

    const UA_ByteString trustList[2] = { g_trustMfg, g_trustLog };

    rc = UA_ClientConfig_setDefaultEncryption(
        cc, g_clientCert, g_clientKey,
        trustList, 2,
        NULL, 0
    );
    if(rc != UA_STATUSCODE_GOOD) {
        printf("[MES] Client encryption config failed: 0x%08x\n", (unsigned)rc);
        UA_Client_delete(client);
        return NULL;
    }

    cc->securityMode = UA_MESSAGESECURITYMODE_SIGNANDENCRYPT;
    UA_String_clear(&cc->securityPolicyUri);
    cc->securityPolicyUri = UA_STRING_ALLOC("http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256");

    UA_String_clear(&cc->clientDescription.applicationUri);
    cc->clientDescription.applicationUri = UA_STRING_ALLOC("urn:opcua:mes");
    return client;
}

static UA_StatusCode write_speed(UA_Client *c, const char *nodeStr, double spd) {
    if(spd < 0.0) spd = 0.0;
    if(spd > 100.0) spd = 100.0;

    UA_Variant v; UA_Variant_init(&v);
    UA_Variant_setScalar(&v, &spd, &UA_TYPES[UA_TYPES_DOUBLE]);

    return UA_Client_writeValueAttribute(
        c, UA_NODEID_STRING(1, (char*)nodeStr), &v);
}

/*===========u32노드 읽고 쓰는 헬퍼========*/
static UA_StatusCode read_u32_node(UA_Client *c, const char *nodeStr, UA_UInt32 *out) {
    if(!out) return UA_STATUSCODE_BADINVALIDARGUMENT;
    UA_Variant v; UA_Variant_init(&v);

    UA_StatusCode rc = UA_Client_readValueAttribute(
        c, UA_NODEID_STRING(1, (char*)nodeStr), &v);

    if(rc == UA_STATUSCODE_GOOD && UA_Variant_hasScalarType(&v, &UA_TYPES[UA_TYPES_UINT32])) {
        *out = *(UA_UInt32*)v.data;
    } else {
        rc = (rc == UA_STATUSCODE_GOOD) ? UA_STATUSCODE_BADTYPEMISMATCH : rc;
    }

    UA_Variant_clear(&v);
    return rc;
}



/* ---- MFG methods ---- */
static UA_StatusCode call_mfg_start(UA_Client *mfg, const char *orderId) {
    UA_Variant in; UA_Variant_init(&in);
    UA_String order = UA_STRING((char*)orderId);
    UA_Variant_setScalar(&in, &order, &UA_TYPES[UA_TYPES_STRING]);

    size_t outSize = 0; UA_Variant *out = NULL;
    UA_StatusCode rc = UA_Client_call(
        mfg,
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_STRING(1, "mfg/StartOrder"),
        1, &in,
        &outSize, &out);
    freeCallOutput(outSize, out);
    return rc;
}

static UA_StatusCode call_mfg_stop(UA_Client *mfg) {
    size_t outSize = 0; UA_Variant *out = NULL;
    UA_StatusCode rc = UA_Client_call(
        mfg,
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_STRING(1, "mfg/StopOrder"),
        0, NULL,
        &outSize, &out);
    freeCallOutput(outSize, out);
    return rc;
}

/* ---- LOG methods ---- */
static UA_StatusCode call_log_move(UA_Client *log, UA_UInt16 wh, UA_UInt32 qty) {
    UA_Variant in[2];
    UA_Variant_init(&in[0]);
    UA_Variant_init(&in[1]);
    UA_Variant_setScalar(&in[0], &wh,  &UA_TYPES[UA_TYPES_UINT16]);
    UA_Variant_setScalar(&in[1], &qty, &UA_TYPES[UA_TYPES_UINT32]);

    size_t outSize = 0; UA_Variant *out = NULL;
    UA_StatusCode rc = UA_Client_call(
        log,
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_STRING(1, "log/Move"),
        2, in,
        &outSize, &out);
    freeCallOutput(outSize, out);
    return rc;
}

static UA_StatusCode call_log_stop(UA_Client *log) {
    size_t outSize = 0; UA_Variant *out = NULL;
    UA_StatusCode rc = UA_Client_call(
        log,
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_STRING(1, "log/StopMove"),
        0, NULL,
        &outSize, &out);
    freeCallOutput(outSize, out);
    return rc;
}
static UA_StatusCode call_log_consume(
    UA_Client *log,
    UA_UInt16 wh,
    UA_UInt32 qty)
{
    UA_Variant in[2];
    UA_Variant_init(&in[0]);
    UA_Variant_init(&in[1]);
    UA_Variant_setScalar(&in[0],&wh,&UA_TYPES[UA_TYPES_UINT16]);
    UA_Variant_setScalar(&in[1],&qty,&UA_TYPES[UA_TYPES_UINT32]);

    size_t outSize=0;
    UA_Variant *out=NULL;

    UA_StatusCode rc =
        UA_Client_call(log,
            UA_NODEID_NUMERIC(0,UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_STRING(1,"log/Consume"),
            2,in,
            &outSize,&out);

    freeCallOutput(outSize,out);

    return rc;
}
/*==================fire value====================*/
#define FIRE_ON_TEMP   50.0
#define FIRE_OFF_TEMP  45.0   /* 히스테리시스 */
static int g_fire_alarm = 0;  /* 0=정상, 1=화재 */
static UA_Double g_temp_mfg = -999.0;
static UA_Double g_temp_log = -999.0;
static int g_emergency_stopped = 0;

/*================== fire_detect====================*/
static void fire_eval_and_act(void) {
    UA_Double tmax = g_temp_mfg;
    if(g_temp_log > tmax) tmax = g_temp_log;

    /* 아직 온도 못 받았으면 판단하지 않음 */
    if(tmax < -100.0) return;

    if(!g_fire_alarm && tmax > FIRE_ON_TEMP ) {
        g_fire_alarm = 1;
        g_emergency_stopped = 1;

        printf("[MES] FIRE ALARM ON! Tmax=%.2f (MFG=%.2f, LOG=%.2f)\n",
               (double)tmax, (double)g_temp_mfg, (double)g_temp_log);
        fflush(stdout);

        /* 양쪽 정지 */
        if(g_mfg_client) {
            write_speed(g_mfg_client, "mfg/conveyor_speed", 0.0);
            (void)call_mfg_stop(g_mfg_client);
        }
        if(g_log_client) {
            (void)call_log_stop(g_log_client);   /* 물류 동작 중지 */
        }
    }
}
/* ----------------- LOG WH status+qty aggregated print ----------------- */
static UA_Boolean g_wh_loading[3] = {false,false,false};
static UA_Boolean g_wh_loaded[3]  = {false,false,false};
static UA_UInt32  g_wh_qty[3]     = {0,0,0};
static UA_UInt64 g_last_attempt = 0;





static const char* wh_state_str(int i) {
    if(g_wh_loaded[i]) return "LOADED";
    if(g_wh_loading[i]) return "LOADING";
    return "IDLE";
}
static void print_log_wh_status(void) {
    printf("[LOG] WH1:%s(%u) | WH2:%s(%u) | WH3:%s(%u)\n",
           wh_state_str(0), (unsigned)g_wh_qty[0],
           wh_state_str(1), (unsigned)g_wh_qty[1],
           wh_state_str(2), (unsigned)g_wh_qty[2]);
    fflush(stdout);
}

/* ----------------- subscription callback ----------------- */
/* sensor/production 출력은 필요하면 켜기 */
static int g_print_mfg_sensor = 0;  /* 0=off, 1=on */

static const char* defectCodeStr(UA_UInt16 code) {
    switch(code) {
        case 0: return "OK";
        case 1: return "파손";
        case 2: return "기준미달";
        case 3: return "치수오차";
        default: return "UNKNOWN";
    }
}

/* Production snapshot (optional) */
static UA_UInt64 g_attempt = 0, g_ok = 0, g_defect = 0;
static UA_Double g_defect_rate = 0.0;
static UA_UInt16 g_defect_code = 0;



static void print_prod_line(void) {
    printf("[MFG][PROD] attempt=%llu ok=%llu defect=%llu rate=%.2f%% code=%u(%s)\n",
           (unsigned long long)g_attempt,
           (unsigned long long)g_ok,
           (unsigned long long)g_defect,
           (double)g_defect_rate,
           (unsigned)g_defect_code,
           defectCodeStr(g_defect_code));
    fflush(stdout);
}

static void onDataChange(UA_Client *client, UA_UInt32 subId, void *subCtx,
                         UA_UInt32 monId, void *monCtx, UA_DataValue *value)
{
    (void)client; (void)subId; (void)monId;

    const char *tag = (const char*)monCtx;
    if(!value || !value->hasValue) return;

    UA_Variant *v = &value->value;

    /* ---- LOG WH booleans ---- */
    if(UA_Variant_hasScalarType(v, &UA_TYPES[UA_TYPES_BOOLEAN])) {
        UA_Boolean b = *(UA_Boolean*)v->data;
        if(strcmp(tag,"WH1_LowStock")==0 ||
            strcmp(tag,"WH2_LowStock")==0 ||
            strcmp(tag,"WH3_LowStock")==0)
        {
            UA_Boolean low = *(UA_Boolean*)v->data;

            if(low && !g_emergency_stopped)
            {
                g_emergency_stopped = 1;

                printf("[MES] LOW STOCK → STOP MFG\n");

                write_speed(g_mfg_client,"mfg/conveyor_speed",0);
                call_mfg_stop(g_mfg_client);
            }

            return;
        }
        if(strcmp(tag, "WH1_Loading") == 0) { g_wh_loading[0] = b; print_log_wh_status(); return; }
        if(strcmp(tag, "WH1_Loaded")  == 0) { g_wh_loaded[0]  = b; print_log_wh_status(); return; }
        if(strcmp(tag, "WH2_Loading") == 0) { g_wh_loading[1] = b; print_log_wh_status(); return; }
        if(strcmp(tag, "WH2_Loaded")  == 0) { g_wh_loaded[1]  = b; print_log_wh_status(); return; }
        if(strcmp(tag, "WH3_Loading") == 0) { g_wh_loading[2] = b; print_log_wh_status(); return; }
        if(strcmp(tag, "WH3_Loaded")  == 0) { g_wh_loaded[2]  = b; print_log_wh_status(); return; }

        /* Other booleans */
        printf("[MES][SUB] %s = %s\n", tag, b ? "true" : "false");
        return;
    }

    /* ---- LOG WH qty ---- */
    if(UA_Variant_hasScalarType(v, &UA_TYPES[UA_TYPES_UINT32])) {
        UA_UInt32 u = *(UA_UInt32*)v->data;

        if(strcmp(tag, "WH1_Qty") == 0) { g_wh_qty[0] = u; print_log_wh_status(); return; }
        if(strcmp(tag, "WH2_Qty") == 0) { g_wh_qty[1] = u; print_log_wh_status(); return; }
        if(strcmp(tag, "WH3_Qty") == 0) { g_wh_qty[2] = u; print_log_wh_status(); return; }
        printf("[MES][SUB] %s = %u\n", tag, (unsigned)u);
        return;
    }
    if(UA_Variant_hasScalarType(v, &UA_TYPES[UA_TYPES_DOUBLE])) {
        UA_Double d = *(UA_Double*)v->data;

        if(strcmp(tag, "MFG_Temp") == 0) {
            g_temp_mfg = d;
            /* 필요하면 로그 */
            // printf("[MFG][ENV] Temp=%.2f\n", (double)d);
            fire_eval_and_act();
            return;
        }

        if(strcmp(tag, "LOG_Temp") == 0) {
            g_temp_log = d;
            // printf("[LOG][ENV] Temp=%.2f\n", (double)d);
            fire_eval_and_act();
            return;
        }
    }

    /* ---- MFG production nodes (trigger on Attempt) ---- */
    if(UA_Variant_hasScalarType(v, &UA_TYPES[UA_TYPES_UINT64])) {
        UA_UInt64 u = *(UA_UInt64*)v->data;

        if(strcmp(tag, "Attempt") == 0) {
            if(g_emergency_stopped) return;
            /* subCtx에 LOG client 넣어둠 */
            UA_Client *logc = (UA_Client*)subCtx;

            g_attempt = u;

            /* 증가분만 처리 */
            if(u > g_last_attempt) {
                UA_UInt64 diff = u - g_last_attempt;
                g_last_attempt = u;

                for(UA_UInt64 k=0; k<diff; k++) {
                    if(g_emergency_stopped) break;
                    UA_StatusCode r1 = call_log_consume(logc, 1, 1);
                    UA_StatusCode r2 = call_log_consume(logc, 2, 1);
                    UA_StatusCode r3 = call_log_consume(logc, 3, 1);

                    UA_UInt32 q1=0,q2=0,q3=0;

                    read_u32_node(logc,"log/wh1/load_qty",&q1);
                    read_u32_node(logc,"log/wh2/load_qty",&q2);
                    read_u32_node(logc,"log/wh3/load_qty",&q3);
                    if( (r1 == UA_STATUSCODE_BADOUTOFRANGE || r1 == UA_STATUSCODE_BADINVALIDSTATE) ||
                        (r2 == UA_STATUSCODE_BADOUTOFRANGE || r2 == UA_STATUSCODE_BADINVALIDSTATE) ||
                        (r3 == UA_STATUSCODE_BADOUTOFRANGE || r3 == UA_STATUSCODE_BADINVALIDSTATE) )
                    {
                        if(!g_emergency_stopped) {
                            g_emergency_stopped = 1;
                            printf("[MES] Consume rejected/insufficient → STOP MFG\n");
                            fflush(stdout);

                            write_speed(g_mfg_client, "mfg/conveyor_speed", 0.0);
                            call_mfg_stop(g_mfg_client);
                        }
                        break;
                    }
                    printf("[MES] Remaining Stock → WH1:%u WH2:%u WH3:%u\n",
                        q1,q2,q3);

                    fflush(stdout);
                }

                print_prod_line();
                return;
            }
            

            return;
        }
        if(strcmp(tag, "ProdCount") == 0) { g_ok = u; return; }
        if(strcmp(tag, "DefectCount") == 0) { g_defect = u; return; }

        /* other uint64 */
        printf("[MES][SUB] %s = %llu\n", tag, (unsigned long long)u);
        return;
    }

    if(UA_Variant_hasScalarType(v, &UA_TYPES[UA_TYPES_DOUBLE])) {
        UA_Double d = *(UA_Double*)v->data;

        if(strcmp(tag, "DefectRate(%)") == 0 || strcmp(tag, "DefectRate(%%)") == 0) {
            g_defect_rate = d;
            return;
        }

        /* MFG temp/hum 출력은 옵션 */
        if((strcmp(tag, "Temp") == 0 || strcmp(tag, "Humidity") == 0) && !g_print_mfg_sensor)
           return;

        printf("[MES][SUB] %s = %.2f\n", tag, (double)d);
        return;
    }

    if(UA_Variant_hasScalarType(v, &UA_TYPES[UA_TYPES_UINT16])) {
        UA_UInt16 u = *(UA_UInt16*)v->data;
        if(strcmp(tag, "DefectCode") == 0) { g_defect_code = u; return; }
        printf("[MES][SUB] %s = %u\n", tag, (unsigned)u);
        return;
    }

    printf("[MES][SUB] %s = (unhandled type)\n", tag);
}

/* ----------------- add_mon helper ----------------- */
static void add_mon(UA_Client *c, UA_UInt32 subId, UA_NodeId nid, const char *tag) {
    UA_MonitoredItemCreateRequest req = UA_MonitoredItemCreateRequest_default(nid);
    UA_MonitoredItemCreateResult res =
        UA_Client_MonitoredItems_createDataChange(
            c, subId, UA_TIMESTAMPSTORETURN_BOTH,
            req, (void*)tag, onDataChange, NULL);

    printf("[MES] mon %-14s status=0x%08x monId=%u\n",
           tag, (unsigned)res.statusCode, res.monitoredItemId);
}

/* ----------------- main ----------------- */
int main(void) {
    signal(SIGINT, on_sigint);

    UA_Client *mfg = makeSecureClient();
    UA_Client *log = makeSecureClient();


    if(!mfg || !log) {
        UA_Client_delete(mfg);
        UA_Client_delete(log);
        clear_pki();
        return 1;
    }
    g_mfg_client = mfg; 
    UA_StatusCode rc;

    rc = UA_Client_connectUsername(mfg, "opc.tcp://10.10.16.208:4870", "mes", "mespw_change_me");
    if(rc != UA_STATUSCODE_GOOD) {
        printf("[MES] connect MFG fail 0x%08x\n", (unsigned)rc);
        goto cleanup;
    }

    rc = UA_Client_connectUsername(log, "opc.tcp://127.0.0.1:4860", "mes", "mespw_change_me");
    if(rc != UA_STATUSCODE_GOOD) {
        printf("[MES] connect LOG fail 0x%08x\n", (unsigned)rc);
        goto cleanup;
    }
    g_mfg_client = mfg;
    g_log_client = log;
    /* ---- MFG Subscription ---- */
    UA_CreateSubscriptionRequest reqM = UA_CreateSubscriptionRequest_default();
    reqM.requestedPublishingInterval = 1000.0;

    UA_CreateSubscriptionResponse respM =
        UA_Client_Subscriptions_create(mfg, reqM, log, NULL, NULL);

    if(respM.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        printf("[MES] MFG subscription create fail 0x%08x\n",
               (unsigned)respM.responseHeader.serviceResult);
        goto cleanup;
    }
    UA_UInt32 subM = respM.subscriptionId;

    /* MFG sensor (optional print off) */
    add_mon(mfg, subM, UA_NODEID_STRING(1, "mfg/temp"), "MFG_Temp");
    add_mon(mfg, subM, UA_NODEID_STRING(1, "mfg/hum"), "MFG_Humidity");

    /* MFG production (if server provides these nodes) */
    add_mon(mfg, subM, UA_NODEID_STRING(1, "mfg/attempt_count"), "Attempt");
    add_mon(mfg, subM, UA_NODEID_STRING(1, "mfg/prod_count"), "ProdCount");
    add_mon(mfg, subM, UA_NODEID_STRING(1, "mfg/defect_count"), "DefectCount");
    add_mon(mfg, subM, UA_NODEID_STRING(1, "mfg/defect_rate"), "DefectRate(%)");
    add_mon(mfg, subM, UA_NODEID_STRING(1, "mfg/defect_code"), "DefectCode");

    /* ---- LOG Subscription ---- */
    UA_CreateSubscriptionRequest reqL = UA_CreateSubscriptionRequest_default();
    reqL.requestedPublishingInterval = 200.0;

    UA_CreateSubscriptionResponse respL =
        UA_Client_Subscriptions_create(log, reqL, NULL, NULL, NULL);

    if(respL.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        printf("[MES] LOG subscription create fail 0x%08x\n",
               (unsigned)respL.responseHeader.serviceResult);
        goto cleanup;
    }
    UA_UInt32 subL = respL.subscriptionId;

    /* LOG WH signals */
    add_mon(log, subL, UA_NODEID_STRING(1, "log/temp"), "LOG_Temp");
    add_mon(log, subL, UA_NODEID_STRING(1, "log/hum"),  "LOG_Hum");

    add_mon(log, subL, UA_NODEID_STRING(1, "log/wh1/loading"), "WH1_Loading");
    add_mon(log, subL, UA_NODEID_STRING(1, "log/wh1/loaded"),  "WH1_Loaded");
    add_mon(log, subL, UA_NODEID_STRING(1, "log/wh1/load_qty"), "WH1_Qty");

    add_mon(log, subL, UA_NODEID_STRING(1, "log/wh2/loading"), "WH2_Loading");
    add_mon(log, subL, UA_NODEID_STRING(1, "log/wh2/loaded"),  "WH2_Loaded");
    add_mon(log, subL, UA_NODEID_STRING(1, "log/wh2/load_qty"), "WH2_Qty");

    add_mon(log, subL, UA_NODEID_STRING(1, "log/wh3/loading"), "WH3_Loading");
    add_mon(log, subL, UA_NODEID_STRING(1, "log/wh3/loaded"),  "WH3_Loaded");
    add_mon(log, subL, UA_NODEID_STRING(1, "log/wh3/load_qty"), "WH3_Qty");

    add_mon(log, subL,UA_NODEID_STRING(1,"log/wh1/low_stock"),"WH1_LowStock");
    add_mon(log, subL,UA_NODEID_STRING(1,"log/wh2/low_stock"),"WH2_LowStock");
    add_mon(log, subL,UA_NODEID_STRING(1,"log/wh3/low_stock"),"WH3_LowStock");
    printf("[MES] Ready. Keys: f(fire on),g(off fire_alarm), 1/m (MFG), 7/a/s/d/x (LOG), q(all stop)\n");

    while(g_running) {
        UA_Client_run_iterate(mfg, 50);
        UA_Client_run_iterate(log, 50);

        int k = read_key_nonblock();
        if(k < 0) continue;

        if(k == 'f') {
            g_fire_alarm = 1;
            g_emergency_stopped = 1;

            /* 양쪽 정지 */
            if(g_mfg_client) {
                write_speed(g_mfg_client, "mfg/conveyor_speed", 0.0);
                (void)call_mfg_stop(g_mfg_client);
            }
            if(g_log_client) {
                (void)call_log_stop(g_log_client);   /* 물류 동작 중지 */
            }
            printf("[MES] FIRE ALARM active. Start blocked. Press 'f' after cooling.\n");
            fflush(stdout);
            continue;
        }
        if(k == 'g') {
            UA_Double tmax = g_temp_mfg;
            if(g_temp_log > tmax) tmax = g_temp_log;

            if(!g_fire_alarm) {
                printf("[MES] FIRE not active.\n");
                fflush(stdout);
                continue;
            }

            if(tmax > FIRE_OFF_TEMP) {
                printf("[MES] FIRE RESET blocked. Tmax=%.2f > %.2f\n", (double)tmax, (double)FIRE_OFF_TEMP);
                fflush(stdout);
                continue;
            }

            g_fire_alarm = 0;
            g_emergency_stopped = 0;
            g_last_attempt = 0;

            printf("[MES] FIRE ALARM RESET OK. You can start again.\n");
            fflush(stdout);
        }

        if(k == 'q') {
            write_speed(mfg, "mfg/conveyor_speed", 0.0);
            (void)call_mfg_stop(mfg);
            (void)call_log_stop(log);
            printf("[MES] ALL STOP\n");
        }

        if(k == '1') {
            /* ⭐ 재시작 시 래치 리셋 */
            g_emergency_stopped = 0;
            /* ⭐ Attempt 기준 리셋: 재시작 직후 diff 폭주/누락 방지 */
            g_last_attempt = 0;
            (void)write_speed(mfg, "mfg/conveyor_speed", 75.0);
            UA_StatusCode r = call_mfg_start(mfg, "ORD-001");
            printf("[MES] MFG START rc=0x%08x\n", (unsigned)r);
        }
        if(k == 'm') {
            (void)write_speed(mfg, "mfg/conveyor_speed", 0.0);
            UA_StatusCode r = call_mfg_stop(mfg);
            printf("[MES] MFG STOP rc=0x%08x\n", (unsigned)r);
        }

        if(k == '7') {
            (void)write_speed(log, "log/conveyor_speed1", 60.0);
            (void)write_speed(log, "log/conveyor_speed2", 50.0);
            (void)write_speed(log, "log/conveyor_speed3", 40.0);
            printf("[MES] LOG speeds set (60/50/40)\n");
        }

        if(k == 'a') {
            UA_StatusCode r = call_log_move(log, 1, 10);
            printf("[MES] LOG Move WH1 qty=10 rc=0x%08x\n", (unsigned)r);
        }
        if(k == 's') {
            UA_StatusCode r = call_log_move(log, 2, 20);
            printf("[MES] LOG Move WH2 qty=20 rc=0x%08x\n", (unsigned)r);
        }
        if(k == 'd') {
            UA_StatusCode r = call_log_move(log, 3, 30);
            printf("[MES] LOG Move WH3 qty=30 rc=0x%08x\n", (unsigned)r);
        }
        if(k == 'x') {
            UA_StatusCode r = call_log_stop(log);
            printf("[MES] LOG StopMove rc=0x%08x\n", (unsigned)r);
        }
    }

cleanup:
    UA_Client_disconnect(mfg);
    UA_Client_disconnect(log);
    UA_Client_delete(mfg);
    UA_Client_delete(log);
    clear_pki();
    printf("[MES] shutdown\n");
    return 0;
}