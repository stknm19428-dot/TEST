#include <stdio.h>
#include <open62541/client_subscriptions.h>  // ✅ 이거 추가
#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/plugin/log_stdout.h>
#include <unistd.h>

#include <modbus/modbus.h>

static UA_ByteString loadFile(const char *path) {
    UA_ByteString bs = UA_BYTESTRING_NULL;
    FILE *f = fopen(path, "rb");
    if(!f) return bs;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    bs.data = (UA_Byte*)UA_malloc((size_t)sz);
    bs.length = (size_t)sz;
    fread(bs.data, 1, (size_t)sz, f);
    fclose(f);
    return bs;
}

static void onDataChange(UA_Client *client, UA_UInt32 subId, void *subCtx,
                         UA_UInt32 monId, void *monCtx, UA_DataValue *value)
{
    (void)client; (void)subId; (void)subCtx; (void)monId;

    const char *tag = (const char*)monCtx;

    if(!value || !value->hasValue) return;

    if(UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        UA_Double d = *(UA_Double*)value->value.data;
        printf("[MES][SUB] %s = %.2f\n", tag, d);
    }
   /* ----------------------------- MODBUS Code ------------------------------*/
    if(UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_BOOLEAN])) {
        UA_Boolean b = *(UA_Boolean*)value->value.data;
        printf("[MES][SUB] %s = %s\n", tag, b ? "TRUE" : "FALSE");
        fflush(stdout);
    }
}



/* ----------------------------modbus_write----------------------------------*/

static UA_Boolean g_qx0_1_state = false;

static void write_bool(UA_Client *c, const char *nodeStrId, UA_Boolean v) {
    //node id 만들기
    UA_NodeId nid = UA_NODEID_STRING(1, (char*)nodeStrId);

    // variant 준비 : 값을 저장하는 컨테이너
    UA_Variant val;
    // 함수 내부 포인터/플래그를 안전한 초기 상태 0으로 세팅, 구조체 그냥 선언 시에 쓰레기 값 대비
    UA_Variant_init(&val);
    //
    UA_Variant_setScalar(&val, &v, &UA_TYPES[UA_TYPES_BOOLEAN]);

    // 이 Variant는 Boolean 스칼라 값이고, 값은 v다”
    UA_StatusCode rc = UA_Client_writeValueAttribute(c, nid, &val);

    
    printf("[MES] write %s <- %s rc=0x%08x\n",
           nodeStrId, v ? "TRUE" : "FALSE", (unsigned)rc);
}



/*-----------------------------------------------------------------------------------*/





static void freeCallOutput(size_t outSize, UA_Variant *out) {
    if(!out || outSize == 0)
        return;
    UA_Array_delete(out, outSize, &UA_TYPES[UA_TYPES_VARIANT]);
}

static UA_Client* makeSecureClient(void) {
    UA_ByteString clientCert = loadFile("/home/pi240/opcua_prac/certs/mes/cert.der");
    UA_ByteString clientKey  = loadFile("/home/pi240/opcua_prac/certs/mes/key.der");

    UA_ByteString trustMfg = loadFile("/home/pi240/opcua_prac/certs/mes/trust_mfg.der");
    UA_ByteString trustLog = loadFile("/home/pi240/opcua_prac/certs/mes/trust_log.der");
    const UA_ByteString trustList[2] = { trustMfg, trustLog };

    UA_Client *client = UA_Client_new();
    UA_ClientConfig *cc = UA_Client_getConfig(client);

    UA_StatusCode rc = UA_ClientConfig_setDefault(cc);
    if(rc != UA_STATUSCODE_GOOD) {
        printf("[MES] Client default config failed: 0x%08x\n", rc);
        UA_Client_delete(client);
        return NULL;
    }

    rc = UA_ClientConfig_setDefaultEncryption(
        cc,
        clientCert,
        clientKey,
        trustList, 2,
        (const UA_ByteString*)NULL, 0
    );
    if(rc != UA_STATUSCODE_GOOD) {
        printf("[MES] Client encryption config failed: 0x%08x\n", rc);
        UA_Client_delete(client);
        return NULL;
    }

    /* ✅ 여기서부터 "서버 정책과 100% 일치" 강제 */
    cc->securityMode = UA_MESSAGESECURITYMODE_SIGNANDENCRYPT;

    UA_String_clear(&cc->securityPolicyUri);
    cc->securityPolicyUri =
        UA_STRING_ALLOC("http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256");

    /* appUri도 setDefault/config 함수들이 덮어쓸 수 있어서 마지막에 세팅 */
    UA_String_clear(&cc->clientDescription.applicationUri);
    cc->clientDescription.applicationUri = UA_STRING_ALLOC("urn:opcua:mes");

    return client;
}
static const char* modeStr(UA_MessageSecurityMode m) {
    switch(m) {
        case UA_MESSAGESECURITYMODE_NONE: return "None";
        case UA_MESSAGESECURITYMODE_SIGN: return "Sign";
        case UA_MESSAGESECURITYMODE_SIGNANDENCRYPT: return "SignAndEncrypt";
        default: return "?";
    }
}

static void print_server_endpoints(const char *url) {
    UA_Client *c = UA_Client_new();
    UA_ClientConfig_setDefault(UA_Client_getConfig(c));

    size_t epCount = 0;
    UA_EndpointDescription *eps = NULL;
    UA_StatusCode rc = UA_Client_getEndpoints(c, url, &epCount, &eps);
    printf("[MES][DBG] getEndpoints rc=0x%08x, count=%zu\n", (unsigned)rc, epCount);

    if(rc == UA_STATUSCODE_GOOD) {
        for(size_t i=0; i<epCount; i++) {
            printf("  [%zu] mode=%s policy=%.*s\n", i, modeStr(eps[i].securityMode),
                   (int)eps[i].securityPolicyUri.length, (char*)eps[i].securityPolicyUri.data);
        }
    }

    UA_Array_delete(eps, epCount, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
    UA_Client_delete(c);
}

int main(void) {
    print_server_endpoints("opc.tcp://10.10.16.208:4840");
    UA_Client *mfg = makeSecureClient();
    UA_Client *log = makeSecureClient();
    if(!mfg || !log) return 1;

    UA_StatusCode rc;

    rc = UA_Client_connectUsername(mfg, "opc.tcp://10.10.16.208:4840", "mes", "mespw_change_me");
    if(rc != UA_STATUSCODE_GOOD) { printf("[MES] connect MFG fail 0x%08x\n", rc); return 1; }

    
    rc = UA_Client_connect(log, "opc.tcp://10.10.16.208:4841");
    if(rc != UA_STATUSCODE_GOOD) { printf("[MES] connect LOG fail 0x%08x\n", rc); return 1; }

    /* =========================================================
       ===== [STEP 10] YOU CONTROL (Read / Write / Call) =====
       여기부터가 “원하는 통신 데이터/동작”을 네가 결정하는 부분
       ========================================================= */


    UA_CreateSubscriptionRequest req = UA_CreateSubscriptionRequest_default();
    req.requestedPublishingInterval = 1000.0;

    UA_CreateSubscriptionResponse resp =
        UA_Client_Subscriptions_create(mfg, req, NULL, NULL, NULL);

    if(resp.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        printf("[MES] subscription create fail\n");
        return 1;
    }

    UA_UInt32 subId = resp.subscriptionId;

    UA_NodeId tempId = UA_NODEID_STRING(1, "mfg/temp");
    UA_NodeId humId  = UA_NODEID_STRING(1, "mfg/hum");

    /*-------------------- MODBUS Code--------------------------*/
    UA_NodeId q0 = UA_NODEID_STRING(1, "mfg/qx0_0");
    UA_NodeId q1 = UA_NODEID_STRING(1, "mfg/qx0_1");
    UA_NodeId q2 = UA_NODEID_STRING(1, "mfg/qx0_2");


    UA_MonitoredItemCreateRequest monQ0 = UA_MonitoredItemCreateRequest_default(q0);
    UA_Client_MonitoredItems_createDataChange(
        mfg, subId, UA_TIMESTAMPSTORETURN_BOTH,
        monQ0, (void*)"QX0.0", onDataChange, NULL);

    UA_MonitoredItemCreateRequest monQ1 = UA_MonitoredItemCreateRequest_default(q1);
    UA_Client_MonitoredItems_createDataChange(
        mfg, subId, UA_TIMESTAMPSTORETURN_BOTH,
        monQ1, (void*)"QX0.1", onDataChange, NULL);

    UA_MonitoredItemCreateRequest monQ2 = UA_MonitoredItemCreateRequest_default(q2);
    UA_Client_MonitoredItems_createDataChange(
        mfg, subId, UA_TIMESTAMPSTORETURN_BOTH,
        monQ2, (void*)"QX0.2", onDataChange, NULL);

    /*-----------------------------------------------------------------------*/

    UA_MonitoredItemCreateRequest monT =
        UA_MonitoredItemCreateRequest_default(tempId);

    UA_Client_MonitoredItems_createDataChange(
        mfg, subId, UA_TIMESTAMPSTORETURN_BOTH,
        monT, (void*)"Temp", onDataChange, NULL);

    UA_MonitoredItemCreateRequest monH =
        UA_MonitoredItemCreateRequest_default(humId);

    UA_Client_MonitoredItems_createDataChange(
        mfg, subId, UA_TIMESTAMPSTORETURN_BOTH,
        monH, (void*)"Humidity", onDataChange, NULL);

    /* (2) LOG 재고 Read */
    {
        UA_Variant v; UA_Variant_init(&v);
        rc = UA_Client_readValueAttribute(log, UA_NODEID_STRING(1, "log/stock_raw"), &v);
        if(rc == UA_STATUSCODE_GOOD && UA_Variant_hasScalarType(&v, &UA_TYPES[UA_TYPES_UINT32])) {
            printf("[MES] LOG Stock_Raw = %u\n", *(UA_UInt32*)v.data);
        } else {
            printf("[MES] LOG Stock read fail 0x%08x\n", rc);
        }
        UA_Variant_clear(&v);
    }

/* (3) MFG StartOrder Method Call */
{
    UA_Variant in; UA_Variant_init(&in);
    UA_String order = UA_STRING("ORD-001");
    UA_Variant_setScalar(&in, &order, &UA_TYPES[UA_TYPES_STRING]);

    size_t outSize = 0;
    UA_Variant *out = NULL;

    rc = UA_Client_call(mfg,
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_STRING(1, "mfg/StartOrder"),
        1, &in,
        &outSize, &out);

    printf("[MES] Call StartOrder rc=0x%08x, outSize=%zu\n", (unsigned)rc, outSize);

    /* ✅ 안전 정리 */
    freeCallOutput(outSize, out);
}

/* (4) LOG Move(qty) Method Call */
{
    UA_Variant in; UA_Variant_init(&in);
    UA_UInt32 qty = 10;
    UA_Variant_setScalar(&in, &qty, &UA_TYPES[UA_TYPES_UINT32]);

    size_t outSize = 0;
    UA_Variant *out = NULL;

    rc = UA_Client_call(log,
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_STRING(1, "log/Move"),
        1, &in,
        &outSize, &out);

    printf("[MES] Call Move rc=0x%08x, outSize=%zu\n", (unsigned)rc, outSize);

    /* ✅ 안전 정리 */
    freeCallOutput(outSize, out);
}
    /* 상태 확인 Read */
    {
        UA_Variant v; UA_Variant_init(&v);
        rc = UA_Client_readValueAttribute(mfg, UA_NODEID_STRING(1, "mfg/status"), &v);
        if(rc == UA_STATUSCODE_GOOD && UA_Variant_hasScalarType(&v, &UA_TYPES[UA_TYPES_STRING])) {
            UA_String s = *(UA_String*)v.data;
            printf("[MES] MFG Status = %.*s\n", (int)s.length, (char*)s.data);
        }
        UA_Variant_clear(&v);
    }

    {
        UA_Variant v; UA_Variant_init(&v);
        rc = UA_Client_readValueAttribute(log, UA_NODEID_STRING(1, "log/status"), &v);
        if(rc == UA_STATUSCODE_GOOD && UA_Variant_hasScalarType(&v, &UA_TYPES[UA_TYPES_STRING])) {
            UA_String s = *(UA_String*)v.data;
            printf("[MES] LOG Status = %.*s\n", (int)s.length, (char*)s.data);
        }
        UA_Variant_clear(&v);
    }

    /* ========================================================= */

    /*-------------------------modbus_write_test--------------------------------*/
    /* ---- TEST: Control OpenPLC via OPC UA Write ---- */

    write_bool(mfg, "mfg/qx0_1", true);
    usleep(5000 * 1000); 
    write_bool(mfg, "mfg/qx0_1", false);


    printf("[MES] Listening subscription...\n");
    while(1) {
        UA_Client_run_iterate(mfg, 100);
        
        if(fgets(input, sizeof(input), stdin) == NULL)
            continue;

        if(input[0] == 'q')
            break;

        if(input[0] == '1') {
            g_qx0_1_state = !g_qx0_1_state;  // 상태 반전

            write_bool(mfg, "mfg/qx0_1", g_qx0_1_state);

            printf("QX0.1 is now %s\n",
                g_qx0_1_state ? "ON" : "OFF");

    }
    UA_Client_disconnect(mfg);
    UA_Client_disconnect(log);
    UA_Client_delete(mfg);
    UA_Client_delete(log);
    return 0;
}